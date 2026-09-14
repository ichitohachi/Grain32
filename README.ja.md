# Grain32

After Effects用のCPUフィルムグレイン。8/16/32bpc処理、粒のアニメーション、編集可能なレスポンスカーブを備えています。

**0.15.2・ベータテスト版。** [English](README.md) · [ダウンロード](https://github.com/ichitohachi/Grain32/releases) · [不具合報告](https://github.com/ichitohachi/Grain32/issues/new/choose)

独立して開発しているエフェクトです。AdobeやElement Supplyとの提携はなく、Fast Grainとのピクセル単位の一致は保証しません。

## インストール

Releasesの**Assets**からOS用パッケージをダウンロードしてください。**Source code**は開発者向けで、そのままインストールできません。確認済み環境は各リリースの検証欄を参照してください。

プロジェクトを保存してAEを終了します。既存版はプラグイン検索対象外へバックアップし、インストール済みGrain32が1つだけになるようにしてください。検証にはプロジェクトのコピーを使用してください。

### macOS

1. `Grain32-v0.15.2-macOS-universal.zip`を解凍します。
2. 以下に`Grain32`フォルダを作り、`Grain32.plugin`を丸ごとコピーします。

   ```text
   /Applications/Adobe After Effects 2026/Plug-ins/
   ```

3. AEを起動し、**エフェクト > ノイズ＆グレイン**、またはエフェクト＆プリセットの検索から適用します。

Apple SiliconとIntelのコードを含みます。ビルド対象下限はmacOS 12ですが、AE自体がより新しいOSを要求する場合があります。Intel実機は検証が必要です。アドホック署名で、Appleの公証は未取得です。読み込みがブロックされたら、このリポジトリから取得したことを確認し、macOSの「プライバシーとセキュリティ」の許可手順に従ってください。許可できない場合はメッセージを報告してください。システム全体のセキュリティ機能は無効化しないでください。

### Windows

1. `Grain32-v0.15.2-Windows-x64.zip`を解凍します。
2. 以下に`Grain32`フォルダを作り、`Grain32.aex`をコピーします。

   ```text
   C:\Program Files\Adobe\Adobe After Effects 2026\Support Files\Plug-ins\
   ```

3. AEを起動し、**エフェクト > ノイズ＆グレイン**から適用します。

コピーには管理者権限が必要な場合があります。Windows x64向けで、ARM64向けではありません。Authenticode署名は未取得です。WindowsのAE実機検証にご協力ください。

AE 2025では対応するアプリケーションフォルダに読み替えてください。開発環境はApple SiliconのAE 2026です。それ以外の組み合わせは検証対象です。アンインストールはAEを終了し、インストールしたGrain32だけを取り除きます。

## パラメーター

| 項目 | 初期値／範囲 | 内容 |
| --- | --- | --- |
| Intensity | 22／0〜1000 | 粒の強さ。100超ではノイズ振幅を増やします。 |
| Size | 1.25／0.5〜5 | 粒の大きさ。比較はFull解像度・同じ表示倍率で行います。 |
| Softness | 0／0〜100 | 元画像をぼかさず粒を柔らかくします。0超では負荷が増えます。 |
| Grain Color | 18%／0〜100% | モノクロとカラーの粒を混合します。 |
| Frame Rate | 24／0〜30 | Speedが1のときの毎秒更新回数。0で静止。 |
| Animation Speed | 1／0〜10 | 更新速度の倍率。0で静止、0.5で半速、2で倍速。 |
| Grain Response | カーブ | 選択した入力範囲に応じたかかり方。 |
| Range Mode | Luminance | Luminance、Lightness、Hue、Saturation、Red、Green、Blue、Alpha。 |
| Invert Range | オフ | レスポンスを反転します。 |
| Response Opacity | オフ | オフは粒の振幅、オンは合成量をカーブで制御。 |
| Blend Mode | Overlay | 粒と元画像の合成モード。 |
| Blend Opacity | 100% | 全体の合成量。0で元画像になります。 |

### カーブ操作

**Response > Grain Response**を展開します。4つの境界で平坦部と減衰範囲を調整します。平坦部の中央ハンドルを左右にドラッグすると移動、上下では高さが変わります。四角いベジェハンドルで減衰形状を調整でき、水色の線が端点との関係を示します。ヒストグラムは選択した入力範囲です。

初期形状は0〜0.25が最大で、1に向かって直線的に0へ下がります。**Reset Curve**は範囲・高さ・ベジェ・Invert Rangeを初期化します。Range Mode、Response Opacity、他の粒設定は維持します。キーフレームがある場合は現在時刻の値を変更し、全キーフレームを削除しません。

### 合成とアニメーション

Normal、Intensity 100、Blend Opacity 100%では粒の画像を表示します。Response Opacityがオフの場合、レスポンス0の領域は中間グレーになり、元画像は表示されません。Differenceは元画像と粒のチャンネル別の差の絶対値です。モードによりハイライトの見え方が変わります。

32bpcでは入力RGB全体を0〜1に制限しません。ただし、全モードで元の明るさが保存される意味ではありません。整数出力は対応する画素形式の範囲になります。

Animation Speedは更新頻度を変え、粒同士を時間補間しません。速度のキーフレームは「現在時刻×現在の速度」で計算され、速度の積分ではないため位相が飛ぶ場合があります。0.15.2では時間依存の宣言を修正し、静止画・固定パラメーターでも粒を更新します。

## テストと報告

[テスト項目](docs/TESTING.md)を参照してください。バージョン、AE、OS・CPU、カラー設定、ビット深度、パラメーター、再現手順を添えてIssueを作成してください。日本語・英語とも歓迎です。共有権限のない素材は添付しないでください。

MFRを実装し、コア処理の直列・並列での再現性を自動テストしています。AE実機の安定性を保証するものではありません。特にWindowsでMFRのオン・オフを比較してください。将来の更新で描画結果が変わる場合があるため、既存プロジェクトに使用した版を保管してください。

## ビルド

[Adobe After Effects SDK](https://developer.adobe.com/after-effects/)を別途取得してください。SDKは同梱していません。リリースはSDK 25.6 (61)を対象とし、`AE_SDK_ROOT`に`Examples`を含むフォルダを指定します。

macOSは`clang++`と`Rez`を含むAppleの開発ツールが必要です。

```sh
export AE_SDK_ROOT="/path/to/ae-sdk"
bash scripts/build_mac.sh
clang++ -std=c++17 -O2 -Wall -Wextra -Werror tests/grain_core_tests.cpp -o /tmp/grain32-tests
/tmp/grain32-tests
```

WindowsはVisual Studio 2022のC++ビルドツールとWindows SDKが必要です。**x64 developer PowerShell**で実行します。

```powershell
$env:AE_SDK_ROOT = 'C:\path\to\ae-sdk'
.\scripts\build_windows.ps1
```

出力は`dist/Grain32.plugin`または`dist/Grain32.aex`です。両OSで共通のC++ソースを使います。ビルド検証結果はリリースノートを参照してください。Adobe SDKには別途Adobeの規約が適用され、この公開はSDKの再配布権を付与しません。
