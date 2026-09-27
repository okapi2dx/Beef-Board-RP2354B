# Beef Board RP2354B

Beef Board V2 の基板設計、ファームウェア、Windows 用 Configurator をまとめたプロジェクトです。Configurator は、RP2354B と従来の AT90USB1286 の2種類のマイコンを搭載した基板に対応します。

## Configurator

Beef Board Configurator は、IIDX 系コントローラーの設定・調整・診断を行う Windows アプリです。基板を USB 接続し、入力方式やターンテーブル、LED、キー割り当てを設定できます。コントローラーモニターでボタンやターンテーブルの入力状態も確認できます。

主な機能:

- 入力方式（Joystick / Keyboard）、ターンテーブルの感度・方向・デッドゾーン・入力時間の調整
- ボタン、ターンテーブル、センターバーのLED効果・色・明るさの設定
- キーボード割り当てと物理ボタン配置の変更。B8も割り当て先に選べますが、基板にB8ボタンやLEDはありません
- ボタン、ターンテーブル、ファームウェア診断情報のモニター
- 基板に応じたファームウェア書き込み

## 対応基板

このプロジェクトは、異なる2種類のマイコンを搭載した基板を扱います。

| マイコン／基板 | 説明 | ファームウェア／書き込み |
|---|---|---|
| **RP2354B** | このリポジトリの基板設計とファームウェア。11個のボタン入力、ターンテーブル用エンコーダー、ボタンLED、RGB LED出力に対応 | [UF2](firmware/Beef-Board-RP2354B.uf2)をBOOTSELのUSBドライブへ手動でコピー |
| **AT90USB1286** | 従来のBeef Board基板。Configuratorから設定・診断できるレガシー対応 | [HEX](firmware/Beef-Board-AT90USB1286.hex)をConfiguratorから書き込み。ソースコードはこのリポジトリに含みません |

基板により、使用できる入出力や書き込み方法が異なります。Windows版Configuratorのビルド方法は [`configurator/README.md`](configurator/README.md) を参照してください。ポータブルEXEはリポジトリに含めていません。

## PHOENIXWAN UART

RP2354B基板のUART端子は、**従来型PHOENIXWANのターンテーブルリングとセンターバーLEDを接続して制御する用途**です。ターンテーブルリングはJ42、センターバーはJ43に接続し、ConfiguratorのLED設定に合わせて制御します。このUART機能はAT90USB1286基板にはありません。

UARTプロトコルで設定できる色や発光パターンには制限があります。対応する制御内容と接続上の注意は [`firmware/README.md`](firmware/README.md) を参照してください。実機でのPHOENIXWAN接続確認は未実施です。

## 開発とソースコードの由来

基板、ファームウェア、Configurator の設計・実装・デバッグ・文書作成に AI を活用しています。仕様と設計の採用判断はプロジェクト管理者が行います。実機で確認できている範囲は「状態」と各ファームウェアの README に記載しています。

Configurator は [HWXLR8/beef-board](https://github.com/HWXLR8/beef-board) を基にした Beef Board ソフトウェアのフォーク／派生コードを出発点として、RP2354B 対応を加えています。

## 内容

| 場所 | 内容 |
|---|---|
| `hardware/Beef-Board-RP2354B.kicad_pro` | KiCad 10 プロジェクト |
| `hardware/Beef-Board-RP2354B.kicad_pcb` | 指定された基板の配線整理版 |
| `hardware/Beef-Board-RP2354B.kicad_sch` | 基板に合わせた回路図 |
| `hardware/BEEF-BOARD.pretty/` | プロジェクト用フットプリント |
| `hardware/BOM-budget.csv` | 暫定部品表と概算価格 |
| `hardware/ROUTING.md` | 配線整理の変更量と残る確認事項 |
| `hardware/DRC.json` | 現在の KiCad DRC 結果 |
| `hardware/routing-changes.json` | 採用した配線変更の記録 |
| `firmware/Beef-Board-RP2354B.uf2` | BOOTSEL 書き込み用ファームウェア |
| `firmware/Beef-Board-AT90USB1286.hex` | AT90USB1286 用 HEX ファームウェア |
| `firmware/src/` | ファームウェアのソースコード |
| `configurator/` | RP2354B 対応 Configurator のソース一式（Web UI・Windows デスクトップ版） |

## 電源選択（J2）

J2「POWER SELECT」のショート位置で、USBケーブル1本または2本の使い方を選びます。基板表側のJ2に2.54 mmピッチの3ピンヘッダとショートピンを取り付け、基板の印字「1・2・3」を確認してください。

| 使い方 | J2の設定 | USB接続と給電先 |
|---|---|---|
| USBケーブル1本 | 1–2を短絡 | J1に接続。基板本体、ボタンLED、ターンテーブル、センターバーをJ1から給電 |
| USBケーブル2本 | 2–3を短絡 | J1は基板本体とボタンLED用、J4はターンテーブルとセンターバー用。J4側の電源はショットキーダイオードD1を通ります |

ショートピンは1–2か2–3のどちらか一方だけに取り付けてください。J2の1番はJ1のVBUS、2番はVAUX、3番はD1のカソードです。D1により、J4側からJ1側への電流の逆流を防ぎます。旧JP1は実装しません。ヘッダ、ショートピン、USBケーブルは負荷に合った電流定格のものを選んでください。

J3の端子台は **1番がGND、2番が外部5V入力（EXT_5V）** です。外部5V入力はD1の上流側につながっています。接続前に基板のピン番号を確認してください。

## 書き込み

J61（BOOTSEL_SW–GND）を短絡して J60（RUN–GND）でリセットするか、接続済みファームウェアから BOOTSEL コマンドを送ります。J1 の USB 接続で ROM の USB ドライブが表示されたら `firmware/Beef-Board-RP2354B.uf2` をコピーしてください。J61 の短絡は USB ドライブが現れたら外します。

Configurator は [`configurator/README.md`](configurator/README.md) に従ってソースからビルドできます。RP2354B 用のファームウェア選択と BOOTSEL 案内があり、旧 AT90USB1286 も引き続き扱えます。STM32 用の書き込み経路は含みません。
