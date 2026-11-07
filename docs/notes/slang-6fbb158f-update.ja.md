# slang 6fbb158f への依存の更新

[English](slang-6fbb158f-update.md) · 2026-11-08。

未改変のフロントエンドを `6fbb158f156436a1cb4d0ce9de5373a416a05d4c` に固定します。2026-11-08 に確かめた上流の
`master` の先頭で、[`d02b7cda`](slang-d02b7cda-update.ja.md) の 14 コミット後です。上流の CI はこのコミットで
通っています（19 項目）。kirimasim から報告した [slang #2093](https://github.com/MikePopoloski/slang/issues/2093)
の修正を含みます。program の parameter を、`defparam` などで外から指せるようになりました（外から指せないのは、
そのネットと変数だけです）。ほかの変更は、左寄せの書式の欄を右側で埋めること、列挙の要素の範囲を整数の
リテラルに限ること（6.19 の文法どおりに誤りになりました）、仮想インタフェースの再帰、スコープ付きの自己参照、
クラスの遅い特殊化で反復子が無効になることの修正です。

slang の MIT の文面は変わらず、slang 自身の依存も変わりません
（[ライセンスの確認](../measurements/2026-11-08/slang-6fbb158f-licenses.json)）。新しい依存や、新しく入れる
道具はありません。

## 取り込み

- `GenericClassDefSymbol::specializations()` が特殊化の一覧の写しを返すようになったので、
  `Lowerer::hasGenericClassStatics` は型へのポインタとして受け取ります。kirimasim は以前からメンバを問い合わせる
  前に一覧を写していました。front のほかの部分は変わりません。
- slang の「global clocking が無い」（14.14）は、診断 `missing-global-clocking`、既定で誤りの重さを持つ警告に
  なりました。kirimasim は今までどおり、同じ文言と終了コード 2 でこの設計を拒みます。`--compat=vcs` でも
  拒み、slang が受け付ける `-Wno-missing-global-clocking` を渡したときだけ止まらなくなります（2026-11-08 に
  確認）。[現行 pin の一覧](../measurements/2026-11-08/compatibility-6fbb158f-profiles.json)は、これを default と
  vcs の誤りの並びに記録しています。プロファイルの compilation と analysis のフラグは変わりません。

## 新旧の比較

どちらも同じ kirimasim のソース（main `33b93822`）の dev の組み立てで、新しいほうに pin と上の取り込みを
入れました（[比較](../measurements/2026-11-08/slang-6fbb158f-comparison.json)）。

- 自前のテスト: 新しい pin でも **13,617** 件すべて合格。
- 固定した P1 の母集団（kirimasim-tests `1fd46b2a`、1,466 件）: どちらも **1,456 件合格、失敗 0、未対応 10**
  で、各件の状態も同じ。
- `15490e95` の sv-tests: どちらも **344 件中 253 件**で、変わった結果はなし。
- 拡張のプローブ **188** 件は、すべて状態と出力が同じ（[プローブ](../measurements/2026-11-08/slang-6fbb158f-probes.json)）。
- #2093 の再現例（`parameter int Q = 1` を持つ program `prg` を置いたモジュールで `defparam p.Q = 9`）は、古い
  pin では拒まれ、新しい pin では `Q=9` と表示します。

## 未確認

- 新しい pin での CI の Linux のジョブ。push したときに走ります。
