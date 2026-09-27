# Beef Board RP2354B Configurator

RP2354B と従来の AT90USB1286 に対応した、Beef Board 用 Configurator のソースです。Windows デスクトップアプリと Web UI を含みます。

## できること

- 入力方式、ターンテーブル、LED、キー割り当ての設定
- ボタンやターンテーブル入力、ファームウェア診断情報のモニター
- AT90USB1286 用 HEX の書き込み
- RP2354B 用 UF2 の取得と BOOTSEL モードへの切り替え案内

RP2354B 用 UF2 は、表示された ROM USB ドライブに手動でコピーしてください。Configurator は UF2 をドライブへ自動転送しません。STM32 用の接続・書き込み機能はありません。

## 開発とソースコードの由来

開発では、設計、実装、デバッグ、ドキュメント作成に AI を活用しています。仕様や変更の採用判断はプロジェクト管理者が行います。実機での確認状況はこのREADMEに記載します。

Configurator は [HWXLR8/beef-board](https://github.com/HWXLR8/beef-board) を基にした Beef Board ソフトウェアのフォーク／派生コードを出発点として、RP2354B 対応を加えています。ライセンスは [`LICENSE`](LICENSE) を参照してください。

RP2354B は USB HID の `FEED:0003` と Board Info report 6 で識別します。

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
