# Codex 订阅手册（iPhone / Android / 网页）

> 整理日期：2026-08-18  
> 适用：能访问外网的个人用户。  
> 目标：开通 Codex。Codex 没有单独会员，订 ChatGPT Plus / Pro 即可。

---

## 卡到底能不能用

付款入口不同，收单方也不同，用哪张卡跟着入口走。

| 付款入口 | 大陆银联 | 用什么付 |
| --- | --- | --- |
| chatgpt.com | 过不了 | 大陆、香港、澳门以外地区发行的信用卡或借记卡 |
| Google Play | 付款资料选新加坡时可以用 | 新加坡档：银联信用卡，储蓄卡有人能过但更贵。美国档：Visa / Master / AMEX |
| App Store | 绑不上 | 不绑卡。美区 Apple ID 充礼品卡，用余额买 |

网页订阅验的是**发卡地区**。手机内购验的是 **Google / Apple 的付款资料和商店地区**。手里只有大陆卡，就不要走网页，用下面的 Play 或 App Store。

---

## 选哪条路

先用免费账号登录 [Codex](https://openai.com/codex/) 试一下。免费和 Go 目前也能用，不确定再充。

| 你的情况 | 走哪条 |
| --- | --- |
| 安卓，有银联卡 | **Google Play + 新加坡付款资料** |
| 安卓，有 Visa / Master / AMEX | **Google Play + 美国付款资料**（通常比新加坡便宜） |
| 只有 iPhone | **美区 Apple ID + 礼品卡**，App 内购 |
| 已有海外发行的真卡，想网页付款 | **chatgpt.com** 绑这张卡 |

网页、iPhone、安卓付的是同一个 OpenAI 账号，权益会同步。

套餐（美区标价，其他区会变）：

- Plus：$20 / 月
- Pro 5x：$100 / 月
- Pro 20x：$200 / 月

各地区现价可查：[appstoreprice.org](https://appstoreprice.org/zh/apps) 、 [app.vbr.me](https://app.vbr.me/)

---

## 所有路径先做的事

1. 固定用一个 OpenAI 账号。后面网页、手机、Codex 客户端都登录它。
2. 若要求验证手机号：用自己能长期收到短信的号码。不要用接码平台，下次再验证会很麻烦。
   - 国行手机没有原生 eSIM：淘宝 / 拼多多买可编程 SIM（例如 [9esim](https://www.9esim.com/zh/)），再买境外 eSIM 套餐，扫码写入。
   - 安卓能直接读写；iPhone 只能切换，写入要另配写卡器。
3. 登录后打开 Authenticator 两步验证和 Passkey。
4. 注册外区账号、打开 chatgpt.com、切商店地区时，用**对应国家**的 VPN 全局模式。ChatGPT 提示「该地区服务不可用」时，美国节点不行就换新加坡。
5. 新号在 App 里往往不能直接开 Pro：先 Plus，再升级。
   - App Store：升 Pro 后 Plus 的钱会退回，一般 1–2 天。
   - Google Play：不用退 Plus，系统补差价。
6. 付完后打开 <https://chatgpt.com/codex>，能进页面、能下客户端就算通。状态没刷新就登出再登，不要重复付款。用量可看 <https://codexradar.com/>。

---

## 路径 1：安卓 + Google Play

### 装 App

1. 手机应用市场安装 [Google Play](https://play.google.com/store/games)。
2. 在 Play 里安装 [ChatGPT](https://play.google.com/store/apps/details?id=com.openai.chatgpt)。

真机或模拟器里 Play 已装、但 ChatGPT 内购提示不可用：用电脑打开上面的 Play 链接，选「推送到设备」，不要只在手机里搜一次。

### 建付款资料

浏览器打开 [Google Payments 设置](https://payments.google.com/gp/w/home/settings)，新建个人资料：

1. 国家选 **新加坡**（银联）或 **美国**（国际卡）。
2. Address 必填。可用地址生成器，例如 <https://www.meiguodizhi.com/>。
3. Name 填你的真实姓名拼音。
4. 把当前 COUNTRY / REGION 切到这份新资料。多个资料并存时，App 里可能走不到账单页，只留一个。

美国免税州常见：Alaska、Delaware、Montana、New Hampshire、Oregon。账单地址和之后上网用的 IP 不必相同。

先填地址再填卡。先填卡再改地区，卡信息会被清空。

### 绑卡并订阅

1. Google Play → 头像 → 付款和订阅 → 支付方式 → 添加信用卡或借记卡。
2. 打开 ChatGPT → 头像 → 选 Plus 或 Pro → 用 Google Play 支付。
3. 第一次报 `OR-REH-04`：等两天再付。
4. 收据：[Google Payments 交易记录](https://payments.google.com/gp/w/home/activity) → 对应订单 → Download tax invoice。

---

## 路径 2：iPhone + 美区礼品卡

App Store 不收大陆发行的卡，所以用礼品卡充余额，再在 ChatGPT App 里扣余额。

### 注册美区 Apple ID

1. 打开 <https://account.apple.com/account>
2. 自己注册，不要买现成号。
3. 一个地区一个号，不要来回切地区。
4. 挂美国节点，用常用程度低的浏览器开无痕窗口。
5. Apple 不认邮箱别名（`+`、中间加点）。需要多号就去 <https://account.live.com/names/manage> 开真正的 Outlook 邮箱。
6. 手机号可以填 +86。
7. 姓名和账单姓名填真实姓名拼音。注册时没填地址也行，第一次付款再填：真名拼音 + 当地地址。

### 买礼品卡并充值

支付宝目前只卖美区卡：

1. 支付宝搜「惠出境」，或左上角切到旧金山 → Coupons → 大牌礼卡。
2. 选美国任意城市 → Pockyt Shop → Apple Gift Card US。
3. 在 iPhone 的 **App Store**（不是系统设置）退出国区号，登录美区号。系统 Apple ID 可以继续用国区。
4. 头像 → 使用礼品卡 → 输入卡密。

也可以在 Apple 官网用已绑 Apple Pay 的 Visa 买：

- 美国：<https://www.apple.com/shop/buy-giftcard/giftcard>
- 日本：<https://www.apple.com/jp/shop/buy-giftcard/giftcard>

用 iPhone Safari 打开对应地区官网，收发件人都填自己，账单地址改成当地地址。

### 订阅

1. 同一台 iPhone 不要登录多个新 Apple ID。
2. 订阅前挂美国节点。
3. App Store 下载 ChatGPT，登录你的 OpenAI 账号，点 Upgrade，先 Plus。
4. 付款框确认是 Apple Account Balance。

新号首次订阅常提示 `Purchase could not be completed`：

1. 打开 <https://getsupport.apple.com/products>
2. App Store → 订阅和购买 → 无法购买 → 获取更多协助 → 在线聊。
3. 等 48 小时。期间不要再点订阅，否则冷却重算。

收据：两天内查邮箱。没有就到 App Store → Purchase History → Resend Receipt。还没有就找苹果客服。

---

## 路径 3：网页 + 海外发行的卡

只在你已经有一张**大陆 / 香港 / 澳门以外地区发行**的信用卡或借记卡时用。大陆银联、大陆 Visa 走这条通常会被拒。

1. 开对应国家的 VPN。
2. 打开 <https://chatgpt.com/> → 头像 → Upgrade Plan。
3. 选套餐并绑上面那张卡。卡上姓名、预留手机号按办卡时填写；账单姓名用真名拼音。
4. 账单地址按页面要求填。美国档可用免税州地址，例如 [Oregon](https://www.meiguodizhi.com/usa-address/oregon)。
5. 付款后：ChatGPT 设置 → 账单 → 付款历史 → 下载 Receipt。

地区选择以当时页面为准。低价区风控更紧，付失败就换日本、加拿大或美国，不要连点。

---

## 常见问题

| 现象 | 处理 |
| --- | --- |
| 网页提示卡被拒 | 这是 Stripe。换海外发行的卡，或改走手机内购 |
| Play 绑不上银联 | 确认付款资料是新加坡，且只留这一份资料 |
| Play 美国档银联失败 | 正常。美国档用 Visa / Master / AMEX |
| 安卓提示该地区不可用 | 换新加坡节点 |
| `OR-REH-04` | 等两天再付 |
| App Store 绑不上大陆卡 | 不要绑。走礼品卡 |
| `Purchase could not be completed` | 找苹果客服，等 48 小时 |
| 升 Pro 后状态没变 | 先登出再登，不要再付一次 |

---

## 一句话

- **安卓 + 银联：Google Play 新加坡档。**
- **安卓 + 国际卡：Google Play 美国档。**
- **iPhone：自己注册美区 Apple ID，支付宝买美区礼品卡，App 里订。**
- **网页：只有海外发行的卡才走；大陆银联不要试网页。**
- **Pro 先 Plus 再升级。**
