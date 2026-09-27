# Beef Board RP2354B Configurator

RP2354B ボード用の設定画面と Windows デスクトップアプリのソース一式です。従来の AT90USB1286 ボードも扱えます。基礎となるコードは [Beef-Board-Configurator](https://github.com/okapi2dx/Beef-Board-Configurator) の `b6ec614c0b2fd3835fd0e333b89df01c0daa031f` から取得し、RP2354B 対応を統合しています。ライセンスは [`LICENSE`](LICENSE) を参照してください。

RP2354B は USB HID の `FEED:0003` と Board Info report 6 で識別します。設定の読み書き、BOOTSEL モードへの切り替え、同梱 UF2 のダウンロードに対応します。UF2 は表示された ROM USB ドライブへ手動でコピーします。STM32 用の接続・書き込み機能は含みません。

ポータブル EXE はリポジトリに含めていません。Windows 64-bit 版は、次の手順でソースからビルドしてください。作成したアプリには RP2354B の UF2 が含まれます。

## ソースからビルド

Node.js 24 と npm を使用します。Web UI のビルド:

```powershell
cd beef-config
npm ci
npm run build
```

Windows デスクトップ版の作成:

```powershell
cd ../desktop
npm ci
npm run dist
```

作成した実行ファイルは `build/` に出力されます。`desktop/` の AVRDUDE 経路は従来の AT90USB1286 用です。RP2354B は `beef-config/static/firmware/Beef-Board-RP2354B.uf2` を使用します。

RP2354B 実機での USB 接続、設定読み書き、BOOTSEL 切り替えは未確認です。
