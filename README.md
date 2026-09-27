# Beef Board RP2354B

Beef Board RP2354B の基板設計、ファームウェア、Windows 用 Configurator をまとめたプロジェクトです。Configurator は、RP2354B と従来の AT90USB1286 の2種類のマイコンを搭載した基板に対応します。

## 開発とソースコードの由来

基板、ファームウェア、Configurator の設計・実装・デバッグ・文書作成に AI を活用しています。仕様と設計の採用判断はプロジェクト管理者が行います。実機での確認状況は各ファームウェアの README に記載しています。

Configurator は [HWXLR8/beef-board](https://github.com/HWXLR8/beef-board) を基にした Beef Board ソフトウェアのフォーク／派生コードを出発点として、RP2354B 対応を加えています。

## Configurator

Beef Board Configurator は、IIDX 系コントローラーの設定・調整・診断を行う Windows アプリです。基板を USB 接続し、入力方式やターンテーブル、LED、キー割り当てを設定できます。コントローラーモニターでボタンやターンテーブルの入力状態も確認できます。

### 主な機能

- **入力設定**
  - Default / IIDX Entry / IIDX Premium モード
  - Joystick / Keyboard 入力切り替え
  - アナログ / デジタルターンテーブル、回転方向の反転
  - 感度、デッドゾーン、入力保持時間、入力ディレイの調整
  - ボタンのチャタリング防止設定
- **LED設定**
  - ボタン、ターンテーブル、センターバーのLED設定
  - 色、発光効果、明るさ、フェード時間の調整
  - ボタンLEDの点灯反転
- **キー割り当て**
  - Keyboard モードで使うキーの変更
  - 物理ボタンと論理ボタンの配置変更
  - B8も割り当て先に選択可能（基板上にB8ボタンやLEDはありません）
- **コントローラーモニター**
  - ボタンとターンテーブルの入力状態をリアルタイム表示
  - ファームウェアから届く診断情報を表示
  - 実機を操作しながら入力調整やトラブルシューティング
- **ファームウェア更新**
  - AT90USB1286 のDFUモードとHEX書き込みに対応
  - avrdude をアプリに同梱し、別途 Python や beef-tool を起動せずに更新
  - HEXファイルの確認、書き込みログと結果の表示
  - RP2354B のUF2取得と BOOTSEL 書き込み案内（USBドライブへのコピーは手動）
- **その他**
  - USB接続状態とファームウェアバージョンの表示
  - アプリ更新の確認
  - 日本語 / English、ライト / ダークテーマ

## 対応基板

このプロジェクトは、異なる2種類のマイコンを搭載した基板を扱います。

| マイコン／基板 | 説明 | ファームウェア／書き込み |
|---|---|---|
| **RP2354B** | このリポジトリの基板設計とファームウェア。11個のボタン入力、ターンテーブル用エンコーダー、ボタンLED、RGB LED出力に対応 | [UF2](firmware/Beef-Board-RP2354B.uf2)をBOOTSELのUSBドライブへ手動でコピー |
| **AT90USB1286** | 従来のBeef Board基板。Configuratorから設定・診断できるレガシー対応 | [HEX](firmware/Beef-Board-AT90USB1286.hex)をConfiguratorから書き込み。ソースコードはこのリポジトリに含みません |

基板により、使用できる入出力や書き込み方法が異なります。Windows版Configuratorのビルド方法は [`configurator/README.md`](configurator/README.md) を参照してください。ポータブルEXEはリポジトリに含めていません。

## 旧PHOENIXWAN UART

RP2354B基板のUART端子は、**旧PHOENIXWANのターンテーブルリングとセンターバーLEDを接続して制御する用途**です。ターンテーブルリングはJ42、センターバーはJ43に接続し、ConfiguratorのLED設定に合わせて制御します。このUART機能はAT90USB1286基板にはありません。

UARTプロトコルで設定できる色や発光パターンには制限があります。対応する制御内容と接続上の注意は [`firmware/README.md`](firmware/README.md) を参照してください。旧PHOENIXWAN実機での接続確認は未実施です。

## 電源選択（J2）

J2「POWER SELECT」のショート位置で、USBケーブル1本または2本の使い方を選べます。

| 使い方 | J2の設定 | 接続方法 |
|---|---|---|
| USBケーブル1本 | 1–2を短絡 | メインUSB端子に接続。基板本体とすべてのLEDに給電 |
| USBケーブル2本 | 2–3を短絡 | メインUSB端子で基板本体とボタンLEDに給電し、もう一方のUSB端子でターンテーブルとセンターバーのLEDに給電 |

J2には2.54 mmピッチの3ピンヘッダとショートピンを取り付けます。基板の印字「1・2・3」を確認し、ショートピンは1–2または2–3のどちらか一方だけに取り付けてください。

## 書き込み

J61（BOOTSEL_SW–GND）を短絡して J60（RUN–GND）でリセットするか、接続済みファームウェアから BOOTSEL コマンドを送ります。J1 の USB 接続で ROM の USB ドライブが表示されたら `firmware/Beef-Board-RP2354B.uf2` をコピーしてください。J61 の短絡は USB ドライブが現れたら外します。

Configurator は [`configurator/README.md`](configurator/README.md) に従ってソースからビルドできます。RP2354B 用のファームウェア選択と BOOTSEL 案内があり、旧 AT90USB1286 も引き続き扱えます。STM32 用の書き込み経路は含みません。
