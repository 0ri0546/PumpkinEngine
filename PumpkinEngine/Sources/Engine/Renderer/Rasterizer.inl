#pragma once

namespace Pumpkin {

    template <FragmentShader_T S>
    void Rasterizer::Rasterize(const PrimitiveBatch& batch, RenderContext& ctx,
        const S& shader, const DrawItem& item) const {
        if (ctx.fb.width == 0 || ctx.fb.height == 0) return;

        const int w = ctx.fb.width;
        const int h = ctx.fb.height;
        const int tilesX = (w + kTileSize - 1) / kTileSize;
        const int tilesY = (h + kTileSize - 1) / kTileSize;
        const auto tileCount = static_cast<std::uint32_t>(tilesX * tilesY);

        BinData& bins = Detail::FrameBins();
        BinPrimitives(batch, tilesX, tilesY, bins);

        // Each thread grabs the next free tile until none are left.
        std::atomic<std::uint32_t> nextTile{ 0 };

        const std::function<void()> worker = [&] {
            thread_local std::vector<SortItem> scratch;

            for (;;) {
                const std::uint32_t index = nextTile.fetch_add(1, std::memory_order_relaxed);
                if (index >= tileCount) return;

                const int x0 = static_cast<int>(index % tilesX) * kTileSize;
                const int y0 = static_cast<int>(index / tilesX) * kTileSize;
                const Tile tile{ x0, y0, std::min(x0 + kTileSize, w), std::min(y0 + kTileSize, h) };

                // This tile's list is only touched by this thread: sort it in place.
                SortItem* begin = bins.items.data() + bins.tileBegin[index];
                SortItem* split = bins.items.data() + bins.tileSplit[index];
                SortItem* end   = bins.items.data() + bins.tileBegin[index + 1];
                SortItems(std::span<SortItem>{ begin, split }, scratch); // opaque, front to back
                SortItems(std::span<SortItem>{ split, end }, scratch);   // transparent, back to front

                for (const SortItem* it = begin; it != end; ++it) {
                    const Primitive& p = batch[it->index];
                    if (p.transparent) DrawInTile<true>(p, it->index, tile, ctx, shader, item);
                    else               DrawInTile<false>(p, it->index, tile, ctx, shader, item);
                }
            }
        };

        GetPool().Run(worker);
    }

    template <bool Transparent, FragmentShader_T S>
    void Rasterizer::DrawInTile(const Primitive& t, std::uint32_t primitiveId, const Tile& tile,
        RenderContext& ctx, const S& shader, const DrawItem& item) const {
        using namespace Detail;

        const float nearPlane = ctx.camera.nearFrame;
        const float farPlane = ctx.camera.farFrame;

        const auto width = ctx.fb.width;
        float* depth = ctx.db.pixels.data();
        std::uint32_t* pixels = ctx.fb.pixels;

        const std::uint8_t isToon = (item.material && item.material->shader == ShaderId::Toon) ? 1 : 0;

        const int xa = std::max(t.x0, tile.x0), xb = std::min(t.x1, tile.x1);
        const int ya = std::max(t.y0, tile.y0), yb = std::min(t.y1, tile.y1);

        const V::type ramp = V::load(kRamp);
        const V::type one = V::set1(1.0f);
        const V::type inv255 = V::set1(1.0f / 255.0f);
        const V::type kv0 = V::set1(t.kv[0]), kv1 = V::set1(t.kv[1]), kv2 = V::set1(t.kv[2]);

        // Start value of an attribute for each lane: start + dx * (0..7).
        const auto lanes = [&](const Plane& p, float start) {
            return V::add(V::set1(start), V::mul(V::set1(p.dx), ramp));
        };
        const auto step = [](const Plane& p) { return V::set1(p.dx * kLanes); };

        FragmentInput frag;
        frag.i0 = t.i0;
        frag.i1 = t.i1;
        frag.i2 = t.i2;
        frag.primitiveId = primitiveId;
        const bool hasAlbedo = item.material->albedo && item.material->albedo->width > 0 && item.material->albedo->height > 0 && !item.material->albedo->pixels.empty();
        const bool hasNormal = item.material->normal && item.material->normal->width > 0 && item.material->normal->height > 0 && !item.material->normal->pixels.empty();
		const bool hasORM = item.material->orm && item.material->orm->width > 0 && item.material->orm->height > 0 && !item.material->orm->pixels.empty();

        int refW = 0, refH = 0;
        // optimization: if they share the same size, then they share the same uvs / bilinear coordinates
        bool sameSize = true;
        const auto track = [&](bool has, const auto& tex) {
            if (!has) return;
            if (refW == 0) { refW = tex->width; refH = tex->height; }
            else if (tex->width != refW || tex->height != refH) sameSize = false;
        };
        track(hasAlbedo, item.material->albedo);
        track(hasNormal, item.material->normal);
        track(hasORM, item.material->orm);

        for (int y = ya; y < yb; ++y) {
            const auto row = static_cast<float>(y - t.y0);

            auto lo = static_cast<float>(xa - t.x0);
            auto hi = static_cast<float>(xb - t.x0);
            if (!ClipSpan(t.w0.At(row), t.w0.dx, lo, hi)) continue;
            if (!ClipSpan(t.w1.At(row), t.w1.dx, lo, hi)) continue;
            if (!ClipSpan(t.w2.At(row), t.w2.dx, lo, hi)) continue;

            // near / far planes: z is linear on the row
            if (Projection == CameraType::Perspective) {
                if (!ClipSpan(t.z.At(row) - nearPlane, t.z.dx, lo, hi)) continue;
                if (!ClipSpan(farPlane - t.z.At(row), -t.z.dx, lo, hi)) continue;
            }

            float* depthRow = depth + static_cast<std::size_t>(y) * width;
            std::uint32_t* pixelRow = pixels + static_cast<std::size_t>(y) * width;

            int x = t.x0 + static_cast<int>(lo);
            const int end = t.x0 + static_cast<int>(hi);
            frag.y = y;

            alignas(32) float aR[kLanes], aG[kLanes], aB[kLanes], aA[kLanes];
            alignas(32) float nX[kLanes], nY[kLanes], nZ[kLanes];
            alignas(32) float mt[kLanes], rg[kLanes], ao[kLanes];
            std::fill_n(aR, kLanes, 1.0f); std::fill_n(aG, kLanes, 1.0f);
            std::fill_n(aB, kLanes, 1.0f); std::fill_n(aA, kLanes, 1.0f);
            std::fill_n(nX, kLanes, 0.0f); std::fill_n(nY, kLanes, 0.0f); std::fill_n(nZ, kLanes, 1.0f);
            std::fill_n(mt, kLanes, 1.0f); std::fill_n(rg, kLanes, 1.0f); std::fill_n(ao, kLanes, 1.0f);

            // ---- SIMD part: 8 pixels per iteration ----
            if (x + kLanes <= end) {
                const auto l = static_cast<float>(x - t.x0);
                V::type vz = lanes(t.z, t.z.At(row, l));
                V::type vk = lanes(t.k, t.k.At(row, l));
                V::type vu = lanes(t.u, t.u.At(row, l));
                V::type vv = lanes(t.v, t.v.At(row, l));
                V::type vw0 = lanes(t.w0, t.w0.At(row, l));
                V::type vw1 = lanes(t.w1, t.w1.At(row, l));
                V::type vw2 = lanes(t.w2, t.w2.At(row, l));
                const V::type sz = step(t.z), sk = step(t.k);
                const V::type su = step(t.u), sv = step(t.v);
                const V::type s0 = step(t.w0), s1 = step(t.w1), s2 = step(t.w2);

                alignas(32) float b0[kLanes], b1[kLanes], b2[kLanes], zs[kLanes];
                alignas(32) float outR[kLanes] = {}, outG[kLanes] = {}, outB[kLanes] = {}, outA[kLanes] = {};

                for (; x + kLanes <= end; x += kLanes) {
                    const V::type oldDepth = V::load(depthRow + x);
                    const V::type mask = DepthMask(item.material->depthTest, vz, oldDepth);
                    const auto bits = V::movemask(mask);

                    // No lane passes: skip the division, the shader and the stores.
                    if (bits != 0) {
                        // Perspective-correct weights: w_i * (1 / w_i) / (sum of them).
                        const V::type inv = V::div(one, vk);
                        V::store(b0, V::mul(V::mul(vw0, kv0), inv));
                        V::store(b1, V::mul(V::mul(vw1, kv1), inv));
                        V::store(b2, V::mul(V::mul(vw2, kv2), inv));
                        V::store(zs, vz);

                        const V::type tu = V::mul(vu, inv), tv = V::mul(vv, inv);
                        alignas(32) float tuLane[kLanes];
                        alignas(32) float tvLane[kLanes];

                        V::store(tuLane, tu);
                        V::store(tvLane, tv);
                        const V::type two = V::set1(2.0f), one1 = V::set1(1.0f);

                        BilinearCoords shared{};
                        if (BilinearFiltering && sameSize && refW > 0)
                            shared = MakeBilinearCoords(refW, refH, tu, tv);

                        auto chan = [&]<int C>(const TexelSIMD & t) { return V::mul(Channel<C>(t), inv255); };

                        if (hasAlbedo) {
                            const Texture& albedo = *item.material->albedo;
                            const auto* albedoPx = albedo.pixels.data();
                            if (BilinearFiltering) {
                                const BilinearTaps k = sameSize
                                    ? GatherTaps(albedoPx, shared)
                                    : GatherBilinear8(albedoPx, albedo.width, albedo.height, tu, tv);
                                const Texel8 bl = BilerpAllChannelsFixed(k);
                                const V::type r = V::mul(bl.r, inv255);
                                const V::type g = V::mul(bl.g, inv255);
                                const V::type b = V::mul(bl.b, inv255);
                                V::store(aR, V::mul(r, r)); // sRGB -> linear
                                V::store(aG, V::mul(g, g));
                                V::store(aB, V::mul(b, b));
                                V::store(aA, V::mul(bl.a, inv255)); // alpha stays linear
                            }
                            else {
                                const TexelSIMD t = SampleTexture8(albedoPx, albedo.width, albedo.height, tu, tv);
                                const V::type r = chan.template operator()<24>(t);
                                const V::type g = chan.template operator()<16>(t);
                                const V::type b = chan.template operator()<8>(t);
                                V::store(aR, V::mul(r, r));
                                V::store(aG, V::mul(g, g));
                                V::store(aB, V::mul(b, b));
                                V::store(aA, chan.template operator()<0>(t));
                            }
                        }
                        if (hasNormal) {
                            const Texture& normal = *item.material->normal;
                            const auto* normalPx = normal.pixels.data();
                            if (BilinearFiltering) {
                                const BilinearTaps k = sameSize
                                    ? GatherTaps(normalPx, shared)
                                    : GatherBilinear8(normalPx, normal.width, normal.height, tu, tv);
                                const Texel8 bl = BilerpAllChannelsFixed(k);
                                const V::type nx = V::sub(V::mul(V::mul(bl.r, inv255), two), one1);
                                const V::type ny = V::sub(V::mul(V::mul(bl.g, inv255), two), one1);
                                const V::type nz = V::sub(V::mul(V::mul(bl.b, inv255), two), one1);

                                const V::type len2 = V::add(V::add(V::mul(nx, nx), V::mul(ny, ny)), V::mul(nz, nz));
                                const V::type invLen = V::div(one1, SqrtMaxEps(len2));
                                V::store(nX, V::mul(nx, invLen));
                                V::store(nY, V::mul(ny, invLen));
                                V::store(nZ, V::mul(nz, invLen));
                            }
                            else {
                                const TexelSIMD t = SampleTexture8(normalPx, normal.width, normal.height, tu, tv);
                                V::store(nX, V::sub(V::mul(chan.template operator()<24>(t), two), one1));
                                V::store(nY, V::sub(V::mul(chan.template operator()<16>(t), two), one1));
                                V::store(nZ, V::sub(V::mul(chan.template operator()<8>(t), two), one1));
                            }
                        }
                        if (hasORM) {
                            const Texture& orm = *item.material->orm;
                            const auto* ormPixels = orm.pixels.data();
                            if (BilinearFiltering) {
                                const BilinearTaps k = sameSize
                                    ? GatherTaps(ormPixels, shared)
                                    : GatherBilinear8(ormPixels, orm.width, orm.height, tu, tv);
                                const Texel8 bl = BilerpAllChannelsFixed(k);
                                V::store(ao, V::mul(bl.r, inv255));
                                V::store(rg, V::mul(bl.g, inv255));
                                V::store(mt, V::mul(bl.b, inv255));
                            }
                            else {
                                const TexelSIMD t = SampleTexture8(ormPixels, orm.width, orm.height, tu, tv);
                                V::store(ao, chan.template operator()<24>(t));
                                V::store(rg, chan.template operator()<16>(t));
                                V::store(mt, chan.template operator()<8>(t));
                            }
                        }

                        // The shader is scalar: call it for the lanes that passed.
                        for (int lane = 0; lane < kLanes; ++lane) {
                            if (!(bits & (1 << lane))) continue;
                            ctx.toonMask[y * width + x + lane] = isToon;

                            frag.x = x + lane;
                            frag.depth = zs[lane];
                            frag.b0 = b0[lane];
                            frag.b1 = b1[lane];
                            frag.b2 = b2[lane];

                            frag.u = tuLane[lane];
                            frag.v = tvLane[lane];

                            frag.albedo[0] = aR[lane]; frag.albedo[1] = aG[lane];
                            frag.albedo[2] = aB[lane]; frag.albedo[3] = aA[lane];
                            frag.nx = nX[lane]; frag.ny = nY[lane]; frag.nz = nZ[lane];
                            frag.metallic = mt[lane]; frag.roughness = rg[lane]; frag.ao = ao[lane];

                            const PixelOutput c = shader(frag);
                            outR[lane] = c.r; outG[lane] = c.g; outB[lane] = c.b; outA[lane] = c.a;
                        }

                        V::type sr = V::load(outR);
                        V::type sg = V::load(outG);
                        V::type sb = V::load(outB);
                        V::type sa = V::load(outA);

                        if constexpr (!Transparent) {
                            V::store(depthRow + x, V::select(oldDepth, vz, mask)); // only opaque writes depth
                            StoreMasked(pixelRow + x, PackBGRA(sr, sg, sb, V::set1(255.0f)), mask);
                        }
                        else {
                            const TexelSIMD old = LoadPixels(pixelRow + x);

                            const V::type a = Clamp01(V::mul(sa, inv255));
                            const V::type half = V::set1(0.5f);
                            const auto mix = [&](V::type d, V::type s) {
                                return V::add(Blend(item.material->blend, d, s, a), half);
                            };

                            const TexelSIMD packed = PackBGRA(
                                mix(Channel<16>(old), sr),
                                mix(Channel<8>(old), sg),
                                mix(Channel<0>(old), sb),
                                mix(Channel<24>(old), sa));
                            StoreMasked(pixelRow + x, packed, mask);
                        }
                    }

                    vz = V::add(vz, sz); vk = V::add(vk, sk);
                    vu = V::add(vu, su); vv = V::add(vv, sv);
                    vw0 = V::add(vw0, s0); vw1 = V::add(vw1, s1); vw2 = V::add(vw2, s2);
                }
            }

            // ---- Scalar tail: the 0..7 pixels left ----
            const auto tailLo = static_cast<float>(x - t.x0);
            float z = t.z.At(row, tailLo);
            float k = t.k.At(row, tailLo);
            float u = t.u.At(row, tailLo);
            float v = t.v.At(row, tailLo);
            float w0 = t.w0.At(row, tailLo);
            float w1 = t.w1.At(row, tailLo);
            float w2 = t.w2.At(row, tailLo);

            for (; x < end; ++x) {
                if (DepthPass(item.material->depthTest, z, depthRow[x])) {
                    const float inv = 1.0f / k;
                    const float tu = u * inv, tv = v * inv;

                    frag.u = tu;
                    frag.v = tv;

                    frag.x = x;
                    frag.depth = z;
                    frag.b0 = w0 * t.kv[0] * inv;
                    frag.b1 = w1 * t.kv[1] * inv;
                    frag.b2 = w2 * t.kv[2] * inv;

                    // Neutral defaults: a missing map costs nothing.
                    frag.albedo[0] = frag.albedo[1] = frag.albedo[2] = frag.albedo[3] = 1.0f;
                    frag.nx = 0.0f; frag.ny = 0.0f; frag.nz = 1.0f;
                    frag.metallic = 1.0f; frag.roughness = 1.0f; frag.ao = 1.0f;

					// One channel (ORM : Occlusion Roughness Metallic) of a map, 0..1 range.
                    const auto sampleR = [&](const Texture& tex) {
                        if (BilinearFiltering)
                            return SampleTextureBilinearChannel<24>(tex, tu, tv) / 255.0f;
                        return static_cast<float>(SampleTexture(tex, tu, tv).r & 0xFF) / 255.0f;
                    };
                    const auto sampleG = [&](const Texture& tex) {
                        if (BilinearFiltering)
                            return SampleTextureBilinearChannel<16>(tex, tu, tv) / 255.0f;
                        return static_cast<float>(SampleTexture(tex, tu, tv).g & 0xFF) / 255.0f;
                    };
                    const auto sampleB = [&](const Texture& tex) {
                        if (BilinearFiltering)
                            return SampleTextureBilinearChannel<8>(tex, tu, tv) / 255.0f;
                        return static_cast<float>(SampleTexture(tex, tu, tv).b & 0xFF) / 255.0f;
                    };

                    if (hasAlbedo) {
                        float r, g, b, a;
                        if (BilinearFiltering) {
                            const TexelF texel = SampleTextureBilinear(*item.material->albedo, tu, tv);
                            r = texel.r / 255.0f; g = texel.g / 255.0f;
                            b = texel.b / 255.0f; a = texel.a / 255.0f;
                        }
                        else {
                            const Color texel = SampleTexture(*item.material->albedo, tu, tv);
                            r = static_cast<float>(texel.r & 0xFF) / 255.0f;
                            g = static_cast<float>(texel.g & 0xFF) / 255.0f;
                            b = static_cast<float>(texel.b & 0xFF) / 255.0f;
                            a = static_cast<float>(texel.a & 0xFF) / 255.0f;
                        }
                        frag.albedo[0] = r * r; // sRGB -> linear
                        frag.albedo[1] = g * g;
                        frag.albedo[2] = b * b;
                        frag.albedo[3] = a; // alpha stays linear
                    }
                    if (hasNormal) {
                        float nx, ny, nz;
                        if (BilinearFiltering) {
                            const TexelF n = SampleTextureBilinear(*item.material->normal, tu, tv);
                            nx = n.r / 255.0f * 2.0f - 1.0f;
                            ny = n.g / 255.0f * 2.0f - 1.0f;
                            nz = n.b / 255.0f * 2.0f - 1.0f;

                            const float invLen = 1.0f / std::sqrt(std::max(nx * nx + ny * ny + nz * nz, 1e-8f));
                            nx *= invLen; ny *= invLen; nz *= invLen;
                        }
                        else {
                            const Color n = SampleTexture(*item.material->normal, tu, tv);
                            nx = static_cast<float>(n.r & 0xFF) / 255.0f * 2.0f - 1.0f;
                            ny = static_cast<float>(n.g & 0xFF) / 255.0f * 2.0f - 1.0f;
                            nz = static_cast<float>(n.b & 0xFF) / 255.0f * 2.0f - 1.0f;
                        }
                        frag.nx = nx; frag.ny = ny; frag.nz = nz;
                    }
                    if (hasORM) {
                        frag.ao = sampleR(*item.material->orm);
                        frag.roughness = sampleG(*item.material->orm);
                        frag.metallic = sampleB(*item.material->orm);
                    }

                    ctx.toonMask[y * width + x] = isToon;
                    const PixelOutput c = shader(frag);
                    const float sr = c.r, sg = c.g, sb = c.b, sa = c.a;

                    if constexpr (!Transparent) {
                        depthRow[x] = z;
                        pixelRow[x] = PackColor(ToByte(sr), ToByte(sg), ToByte(sb));
                    }
                    else {
                        const std::uint32_t pixel = pixelRow[x];
                        const float a = std::clamp(sa / 255.0f, 0.0f, 1.0f);
                        const auto da = static_cast<float>((pixel >> 24) & 0xFF);
                        const auto dr = static_cast<float>((pixel >> 16) & 0xFF);
                        const auto dg = static_cast<float>((pixel >> 8) & 0xFF);
                        const auto dB = static_cast<float>(pixel & 0xFF);
                        pixelRow[x] = PackColor(
                            ToByte(Blend(item.material->blend, dr, sr, a) + 0.5f),
                            ToByte(Blend(item.material->blend, dg, sg, a) + 0.5f),
                            ToByte(Blend(item.material->blend, dB, sb, a) + 0.5f),
                            ToByte(Blend(item.material->blend, da, sa, a) + 0.5f));
                    }
                }

                // Step to the next pixel.
                z += t.z.dx; k += t.k.dx;
                u += t.u.dx; v += t.v.dx;
                w0 += t.w0.dx; w1 += t.w1.dx; w2 += t.w2.dx;
            }
        }
    }
}
