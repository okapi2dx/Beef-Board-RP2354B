# RP2354B 疑似接続エミュレーター

リリースページの [`Beef-Board-RP-Emulator-V1.00.exe`](https://github.com/okapi2dx/Beef-Board-RP2354B/releases/download/V1.00/Beef-Board-RP-Emulator-V1.00.exe) を起動すると、RP2354Bとして疑似接続した設定アプリを表示します。インストールは不要です。

- 起動時はLED設定画面を表示します。ウィンドウサイズは通常アプリと同じ980×740です。
- 上部の「入力パネル」からB1〜B7、E1〜E4、ターンテーブル位置、正転・逆転の回転デモを操作します。
- 設定と旧PhoenixwanのON／OFFは、エミュレーター専用の領域に保存します。
- 「疑似USBを切断／接続」で接続状態を再現します。「設定を初期化」はエミュレーター内の設定だけを初期化します。
- 旧Phoenixwanの−／＋操作はパネルのコマンド履歴で確認できます。
- 色相・彩度・明るさは常に表示し、選択中のモードで調整できない項目はグレーアウトします。

これはアプリ内のHID疑似接続です。Windowsに仮想USB機器やBOOTSELドライブを作成する機能はありません。実機・通常のConfiguratorの設定に影響しません。ファームウェア書き込みは行いません。入力処理の物理的な遅延、デバウンスやUART発光の正確な再現には対応しません。

## ビルド

既存の `configurator/desktop` の依存関係を使います。リポジトリーのルートから実行してください。

```powershell
npm --prefix configurator/desktop run build:ui
npm --prefix configurator/rp-emulator run dist
```

実行ファイルは `configurator/rp-emulator/dist/Beef-Board-RP-Emulator-V1.00.exe` に生成されます。
