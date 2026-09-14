# Beta test checklist / ベータテスト項目

Use a copy of your project. Record Grain32 version, AE version, OS/CPU, bit depth, working space/view transform, resolution, and MFR setting.

プロジェクトのコピーで検証し、Grain32・AEのバージョン、OS・CPU、ビット深度、作業色空間・表示変換、解像度、MFR設定を記録してください。

- [ ] Installation: one Grain32 appears in Noise & Grain; controls and histogram draw correctly. / 重複なく認識され、UIが表示される。
- [ ] Still footage: default grain changes across frames; replaying the same frame gives the same pattern. / 静止画でも粒が動き、同じフレームの結果は再現する。
- [ ] Frame Rate 0 and Animation Speed 0 each freeze grain. Speeds 0.5, 1 and 2 change update frequency. / 静止・速度変更を確認。
- [ ] Intensity 0 and Blend Opacity 0 each return the source. Values 22, 100 and 1000 work. / 無効化と強度の上限を確認。
- [ ] Size 0.5, 1.25 and 5; Softness 0 and 100; Grain Color 0 and 100. / 粒径、柔らかさ、色を確認。
- [ ] Curve boundaries, vertical plateau height, Bézier handles and cyan guides respond to dragging. Reset Curve and Undo work. / カーブの全ハンドル・リセット・Undoを確認。
- [ ] Range Mode, Invert Range and Response Opacity change the intended regions. / 範囲と反転・合成量を確認。
- [ ] Compare Normal, Overlay and Difference with identical input and settings; check all remaining modes for crashes or invalid pixels. / 同一条件で合成モードを比較。
- [ ] Test 8, 16 and 32 bpc, including HDR values above 1 and negative RGB in float projects. / ビット深度とHDR・負値を確認。
- [ ] Test a transparent solid, soft alpha edges, masks, and an adjustment layer. / 透明・半透明・マスク・調整レイヤーを確認。
- [ ] Render the same short sequence with MFR on and off, compare frames, and record render time. / MFRオン・オフで同じ短い連番を比較。
- [ ] Save/reopen the test project and confirm settings and animation persist. / 保存・再読込を確認。

Screenshots should include the controls and viewer zoom/resolution. For a visual comparison, use the same source frame, bit depth, color settings, resolution and zoom. Random patterns need not align pixel-for-pixel.

比較画像は同じ入力フレーム・ビット深度・カラー設定・解像度・表示倍率で撮影してください。ランダムな粒の位置そのものは一致する必要がありません。
