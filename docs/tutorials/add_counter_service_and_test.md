# 从零新增 CounterService：模型、代码生成与双机测试

本教程接续 [新增 client2 与事件订阅](add_client2_and_test.md)。目标是自己定义一个服务接口，打通 ARXML → Proxy/Skeleton → 应用 → 部署 → 双机通信。

## 1. 本轮目标与运行结构

| 成员 | 类型 | 行为 |
| --- | --- | --- |
| `GetCounter()` | 方法，无输入，输出 `value: uint32_t` | 读取当前计数，不修改计数 |
| `CounterChanged` | 事件，载荷 `uint32_t` | 服务端约每秒递增一次并主动发布 |

计数从 0 开始，第一次发布为 1。停止并重新启动 `serverd` 后计数重新开始；`uint32_t` 达到上限后按无符号整数规则回绕，不做持久化。

```text
Machine2 / serverd                         Machine1 / client2d
  CounterServiceImpl                         CounterClient
  counter_PPort                              counter_RPort
       ├─ 每秒递增 → CounterChanged ───────────→ 订阅并读取样本
       └─ 返回当前值 ← GetCounter() ─────────── 仅查询一次
```

新服务运行在已有 `serverd` 中，新客户端逻辑运行在已有 `client2d` 中。因此这一步没有新增进程、软件集或功能组；两端仍由 `HelloWorldGroup.Driving` 启动。原 `EchoMethod` 和 `testEvent` 保留。

## 2. 第一步：定义接口和数据类型

阅读 `samples/helloworld-cm/model/library_common/counter_service.arxml`。

接口完整路径为 `/CounterInterfaces/CounterService`，C++ 命名空间为 `demo::counter`，版本为 `1.0`。

`EVENTS/VARIABLE-DATA-PROTOTYPE` 定义 `CounterChanged`；`METHODS/CLIENT-SERVER-OPERATION` 定义 `GetCounter`，其唯一参数 `value` 的方向为 `OUT`。

二者都引用 SDK 已有标准类型：

```xml
<TYPE-TREF DEST="STD-CPP-IMPLEMENTATION-DATA-TYPE">/AUTOSAR/StdTypes/uint32_t</TYPE-TREF>
```

这个模型会生成 C++ 的 `std::uint32_t`。本轮无需自定义结构体；以后增加多个字段时再学习结构体及序列化。

## 3. 第二步：配置 SOME/IP 部署和事件组

同一文件中的 `/CounterSomeipDeployments/CounterService` 将接口成员映射到网络标识：

| 参数 | 本轮值 | 含义 |
| --- | --- | --- |
| Service ID | 102 / `0x0066` | 区分 CounterService 与原服务 101 |
| Instance ID | 1 | 两端指向同一个服务实例 |
| Major / Minor | 1 / 0 | 接口版本 |
| GetCounter Method ID | 1 | 方法标识 |
| CounterChanged Event ID（ARXML） | `0x0001` | 生成的 SOME/IP 事件标识为 `0x8001` / 32769 |
| Eventgroup1 ID | 1 | 把 CounterChanged 纳入可订阅事件组 |
| Transport | UDP | 方法和事件都使用 UDP |
| 服务端 UDP 端口 | 5002 | 在 Machine2 的实例到机器映射中定义 |

方法、事件组的编号处在各自上下文中；不同服务可以复用这些编号。服务 ID 102 与原服务不同，且新服务端使用端口 5002。

## 4. 第三步：连接端口、实例、进程和机器

以下文件路径均相对于 `samples/helloworld-cm/model`：

| 文件 | 作用 |
| --- | --- |
| `app/server_design.arxml` | 为已有 server SWC 添加 `counter_PPort`，提供新接口 |
| `app/client2_design.arxml` | 为 client2 SWC 添加 `counter_RPort`，要求新接口 |
| `library_for_machine2_integration/counter_deployment.arxml` | Provided 实例、提供的事件组、Machine2 连接器/端口、server 进程端口映射和 Grant |
| `library_for_machine1_integration/counter_deployment.arxml` | Required 实例、要求的事件组、Machine1 连接器、client2 进程端口映射和 Grant |

主要引用关系：

```text
/CounterInterfaces/CounterService
    ↑ /CounterSomeipDeployments/CounterService
    ↑ /CounterDeployment2/ProvidedCounter（服务端）
      /CounterDeployment1/RequiredCounter（客户端）
    ↔ 对应的 P/R port、process、Machine 通信连接器
```

代码中使用的实例说明符由 Executable / 根组件 / Port 组成：

```text
serverd/server/counter_PPort
client2d/client2/counter_RPort
```

客户端通过该说明符调用 `FindService`，由部署映射解析实例，避免选中任意实例。

两端文件还分别定义并引用 Offer、Find、Method、Event Grant。基础机器模型的 IAM 访问控制开关仍为关闭状态，本轮不验证权限拦截。

## 5. 第四步：生成 Proxy 和 Skeleton

项目 CMake 已加载 `library_common`、`app` 及相关部署目录，所以新 ARXML 会由现有 aragen helper 处理。

在 VSCode 开发容器中执行：

```bash
DEMO_DIR="${CAPI_SRC_DIR}/samples/helloworld-cm"
"${SDK_INST_DIR}/ara-sysroot/build.sh" -i "${DEMO_DIR}"
```

本工程已有 `.build` 配置时，也可以使用明确检查退出码的增量命令：

```bash
cmake -S "${DEMO_DIR}" -B "${DEMO_DIR}/.build" &&
cmake --build "${DEMO_DIR}/.build" --parallel 2 &&
cmake --install "${DEMO_DIR}/.build"
```

生成物阅读位置：

```text
.build/server/gen/serverd/includes/demo/counter/counterservice_skeleton.h
.build/client2/gen/client2d/includes/demo/counter/counterservice_proxy.h
```

可以看到 Skeleton 上的虚函数 `GetCounter()`、事件 `CounterChanged`，以及 Proxy 上对应的方法和事件对象。生成文件用于阅读，不手动编辑、不提交到 Git。

## 6. 第五步：实现服务端

阅读 `server/src/counter_service.h`、`counter_service.cpp` 和 `main.cpp`。

1. `CounterServiceImpl` 继承生成的 Skeleton，实现 `GetCounter()`。
2. `GetCounter()` 读取当前计数，通过 `Promise<GetCounterOutput>` 返回 Future。
3. `PublishNext()` 增加计数，再调用 `CounterChanged.Send(value)`。
4. `main.cpp` 创建实例、调用 `OfferService()`，在每秒循环中执行 `PublishNext()`，退出时调用 `StopOfferService()`。

计数采用原子变量，因为 COM 方法处理可能与发布循环位于不同线程。通过共享存储使 Skeleton 能满足生成工厂的移动构造要求。

服务端循环不会等待客户端请求：即使没有客户端，仍每秒更新计数并调用 Send。Send 成功表示发送操作被接受，并不等价于某个客户端已收到；接收证据要看客户端日志。

## 7. 第六步：实现客户端

阅读 `client2/src/counter_client.h`、`counter_client.cpp` 和 `main.cpp`。

`CounterClient::Tick()` 在主循环中约每秒执行一次：

1. 通过 Required port 查询服务；未发现时下轮继续查询。
2. 创建 Proxy，调用 `CounterChanged.Subscribe(8)`。
3. 观察订阅状态，进入 `Subscribed` 后调用 `GetNewSamples` 取出缓存中的样本。
4. 只发送一次 `GetCounter()`，后续循环非阻塞地检查 Future；超时 3 秒则记录错误并退出。
5. 收到方法返回后仍继续读取事件；对象销毁时取消订阅。

这里演示**轮询取样**，与上一节 `testEvent` 的 `SetReceiveHandler` 回调取样形成对照。两种方式都需要订阅和 `GetNewSamples`；8 是样本容量，不是发送频率。轮询会增加接收日志延迟，因此不要要求日志间隔精确等于 1 秒。

原 HelloWorld 方法调用仍使用原 demo 的同步逻辑；完整的断线重连、所有方法的超时治理是后续课题。当前 CounterClient 不主动重建已失效的 Proxy。

## 8. 第七步：部署并手动运行双机测试

以下命令均在 VSCode 开发容器终端执行。更新前先停止此前运行的平台；服务端源码和模型都已改变，必须重建 Machine2 镜像和容器。

完成第 5 节编译安装后，配置 Machine1：

```bash
MACHINE1_IP=$(hostname -I | awk '{print $1}')
"${SDK_INST_DIR}/ara-sysroot/config.sh" \
  -m Machine1 -s "${ARA_SYSROOT}" \
  -a "${CAPI_SRC_DIR}/samples/helloworld-cm" -p "${MACHINE1_IP}"
```

这里沿用 demo 的单共享网络。存在多个网卡时，选择 `capi_shared_network` 对应的 IP。

构建并重建服务端：

```bash
docker compose -f "${CAPI_SRC_DIR}/demo/docker-compose.yml" build capi_demo &&
docker compose -f "${CAPI_SRC_DIR}/demo/docker-compose.yml" up -d --force-recreate capi_demo
docker logs -f --since 1m capi_demo
```

等待平台初始化完成，按 Ctrl+C 停止查看日志，启动服务：

```bash
docker exec capi_demo sh -lc 'sudo "${ARA_SYSROOT}/run.sh" -c HelloWorldGroup.Driving'
docker logs -f --since 1m capi_demo 2>&1 | grep --line-buffered 'CounterServer'
```

先不启动客户端，观察至少 3 条 `CounterServer CounterChanged value:`。这是验证发布不依赖方法请求的第一步。

终端 A 启动 Machine1，保留完整日志：

```bash
sudo "${ARA_SYSROOT}/run.sh" -R 2>&1 | tee /tmp/counter-machine1.log
```

终端 B 在平台就绪后启动 client2：

```bash
sudo "${ARA_SYSROOT}/run.sh" -c HelloWorldGroup.Driving
tail -n 200 -f /tmp/counter-machine1.log |
  grep --line-buffered -E 'CounterClient|Helloworld-cm-Client2.*recv echo|Client2 testEvent'
```

预期看到订阅进入 `Subscribed`、计数持续增加、仅一条 `CounterClient GetCounter value:`；原 EchoMethod 和 `abc` 事件也继续出现。客户端晚启动时首个收到的数值大于 1 属于正常现象。方法读取值与某条事件值不必相等，因为读取、发布、网络接收和轮询发生在不同时间。

持续观察至少 10 秒，再按 Ctrl+C 退出日志查看。通过功能组停止应用以验证清理：

```bash
sudo "${ARA_SYSROOT}/run.sh" -c HelloWorldGroup.Off
docker exec capi_demo sh -lc 'sudo "${ARA_SYSROOT}/run.sh" -c HelloWorldGroup.Off'
docker logs capi_demo > /tmp/counter-machine2.log 2>&1
python3 "${CAPI_SRC_DIR}/samples/helloworld-cm/tests/check_counter_logs.py" \
  /tmp/counter-machine1.log /tmp/counter-machine2.log
```

校验脚本要求一次完整的新启动日志：至少 5 条递增事件、一次方法调用与返回、方法返回后至少 3 条更大的计数、服务端先发布再处理方法，以及两端清理日志。不要混入多次启动记录；此脚本用于短时功能验证，不覆盖计数回绕、UDP 丢包压力或重连。

最后停止平台：

```bash
sudo "${ARA_SYSROOT}/run.sh" -s
docker exec capi_demo sh -lc 'sudo "${ARA_SYSROOT}/run.sh" -s'
```

## 9. 排查顺序

| 现象 | 首先检查 |
| --- | --- |
| 生成文件不存在 | 新 ARXML 是否位于 CMake 加载的目录，app Port 是否引用新接口 |
| ResolveInstanceIDs 失败 | 实例说明符与 Port、Process 映射是否一致，是否重新执行 config.sh |
| 一直没有 CounterClient 日志 | 原 HelloWorld 服务是否已发现，client2 是否处于运行状态 |
| 一直没有发现 CounterService | Machine2 是否重建，CounterServer 是否 Offer，服务 ID/实例/版本是否匹配 |
| 已订阅却无计数 | 服务端周期发布日志、事件组映射、UDP 5002 与两端配置 |
| GetCounter failed | 两端完整日志中的方法请求/响应，服务端是否仍在运行 |
| 事件计数重复从小值开始 | 是否重启过服务端、日志是否混合了多次测试 |

## 10. 本轮实际验证

2026-09-14 完成以下验证：

- 开发容器编译、安装和 Machine1 配置成功；Machine2 镜像重新构建，容器重新创建。
- 生成配置中两端均出现服务 102、实例 1、方法 1、事件 32769 和事件组 1。
- 先启动服务端，未启动客户端时已有连续计数发布。
- client2 收到 30 条递增事件，数值为 13～42；只调用一次 `GetCounter()`，返回 13，随后仍持续收到事件。
- 原 EchoMethod 回包和 `testEvent` 的 `abc` 接收仍正常。
- 两端应用切换到 Off 后完成取消订阅和停止提供服务；日志验收脚本通过。

验收输出：

```text
PASS: 30 increasing events (13..42), one GetCounter=13, independent publishing and cleanup
```

开发容器中的完整日志为 `/tmp/counter-machine1.log` 和 `/tmp/counter-machine2.log`。测试后两端平台已停止。这是一次短时双机功能验证，不代表已经完成断线重连、压力或长时间稳定性测试。
