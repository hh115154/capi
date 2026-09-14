# 新增 client2d 并完成双机通信测试

本文记录在 `samples/helloworld-cm` 中新增客户端 `client2d` 的实际过程，以及 2026-09-14 完成的双机 EchoMethod 测试。目标是让 Machine1 上的新客户端调用 Machine2 上已有的 `serverd`，同时保留原来的 `clientd`。

本文中的命令均在 **VS Code 开发容器终端**执行，除非另有说明。所有步骤按顺序执行；当前项目已经完成新增，不需要重新复制模型或覆盖生成工程。

## 1. 测试拓扑与工具分工

```text
Machine1：VS Code 开发容器
  clientd  ──┐
  client2d ──┴── SOME/IP / Docker 共享网络 ──> Machine2：capi_demo
                                                   serverd
```

| 项目 | 新客户端 | 已有服务端 |
| --- | --- | --- |
| 程序 | `client2d` | `serverd` |
| Machine | `/ISOFT/Development/Machine1` | `/ISOFT/Development/Machine2` |
| Executable FQN | `/Client2App/exe/client2d` | `/ServerApp/exe/serverd` |
| Process FQN | `/Client2Deployment/client2_process` | `/ServerDeployment2/server_process` |
| 软件集 | `CMDemo_client`，与原客户端共用 | `CMDemo_server` |
| 角色 | R-Port / Proxy | P-Port / Skeleton |
| 接口 | `ServiceHelloWorld` | `ServiceHelloWorld` |

双方使用 Service ID `101`、Instance ID `29`、接口版本 `1.0`；`EchoMethod` 的 Method ID 为 `32000`。服务端部署使用 UDP 端口 `5001`。

- `aragen`：根据 ARXML 生成应用工程、通信代码和运行配置。
- SDK 的 `build.sh -i`：编译并安装应用，供后续部署查找。
- SDK 的 `config.sh`：调用 `configMachine`，按目标 Machine 集成程序与配置，并按 `-p` 更新生成配置中的相关 IP 字段。
- 目标运行目录的 `run.sh`：启动、停止平台，查询或切换功能组。

“编译成功”“部署成功”“进程启动”和“通信成功”是四个不同的检查点。

## 2. 环境与前提

需要先完成项目 SDK 的构建、打包和安装。完整基础流程见 [原双机 demo 教程](demo.rst) 和 [项目 README](../../README.rst)。

本次开发容器中使用的路径如下：

| 变量 | 本次值 |
| --- | --- |
| `CAPI_SRC_DIR` | `/workspaces/capi` |
| `SDK_INST_DIR` | `/home/vscode/capi/sdk-native` |
| `ARA_SYSROOT` | `/home/vscode/capi/ara-sysroot` |
| SDK 内部 framework 版本 | `1.0.0` |
| Docker 共享网络 | `capi_shared_network` |

定义后续命令使用的变量：

```bash
DEMO_DIR="${CAPI_SRC_DIR}/samples/helloworld-cm"
SDK_ROOT="${SDK_INST_DIR}/ara-sysroot"
ARAGEN="${SDK_ROOT}/ara/framework/1.0.0/bin/aragen"
BASE_ARXMLS="${SDK_ROOT}/ara/framework/1.0.0/share/ara-arxmls"

printf '源码目录：%s\nSDK目录：%s\n运行目录：%s\n' \
  "${CAPI_SRC_DIR}" "${SDK_ROOT}" "${ARA_SYSROOT}"
test -x "${ARAGEN}" && echo "aragen 可执行文件存在"
```

新开终端后需要重新定义这些辅助变量。宿主机源码路径是 `/home/hh/capi`，不要与容器的 `/workspaces/capi` 混用，也不要跨环境复用 CMake 缓存。

VS Code 工作区使用 `"cmake.cmakePath": "cmake"`，从当前环境的 PATH 查找工具。若要单步调试 CMake 脚本，需要在实际执行配置的环境中安装支持调试的 CMake 3.27 或更新版本，再通过该环境的用户/远程设置指定路径。本次开发容器的默认 CMake 为 3.16.3，不支持 CMake 脚本断点调试；宿主机 `/opt` 下的 CMake 路径在容器内并不存在。

## 3. 文件变更清单

以下文件链接指向本次已经完成的实现，可作为下一次新增客户端的参考。

| 文件 | 本次工作 |
| --- | --- |
| [client2_design.arxml](../../samples/helloworld-cm/model/app/client2_design.arxml) | 新增 ProcessDesign、Executable、根组件、SWC 和 R-Port |
| [client2_deployment.arxml](../../samples/helloworld-cm/model/library_for_machine1_integration/client2_deployment.arxml) | 新增 Process、启动条件、机器与服务实例映射、Grant |
| [service_instances_someip_deployments.arxml](../../samples/helloworld-cm/model/library_for_machine1_integration/service_instances_someip_deployments.arxml) | 新增独立命名的 Required 服务实例 |
| [machine_extension.arxml](../../samples/helloworld-cm/model/library_for_machine1_integration/machine_extension.arxml) | 新增 client2 日志通道 |
| [client_distribution.arxml](../../samples/helloworld-cm/model/library_for_machine1_integration/client_distribution.arxml) | 将新程序和进程加入 `CMDemo_client` |
| [demo CMakeLists.txt](../../samples/helloworld-cm/CMakeLists.txt) | 加入 `add_subdirectory(client2)` |
| [client2/CMakeLists.txt](../../samples/helloworld-cm/client2/CMakeLists.txt) | 配置 ARXML 输入和生成代码依赖 |
| [client2/src/main.cpp](../../samples/helloworld-cm/client2/src/main.cpp) | 实现服务发现与周期方法调用 |

## 4. 新增 ARXML 模型

### 4.1 应用设计

以原来的 `model/app/client_design.arxml` 为参考创建 `client2_design.arxml`，使用独立名称：

```text
/Client2App/Client2Design
    └── Executable：/Client2App/exe/client2d
          └── 根组件：client2
                └── SWC 类型：/Client2App/swc/client2
                      └── R-Port：helloworld_RPort
                            └── /HelloWorldServiceInterfaces/ServiceHelloWorld
```

特别注意根组件的 `APPLICATION-TYPE-TREF`：

```xml
<APPLICATION-TYPE-TREF DEST="ADAPTIVE-APPLICATION-SW-COMPONENT-TYPE">/Client2App/swc/client2</APPLICATION-TYPE-TREF>
```

本次复用已有接口，因此不需要重复定义服务接口或修改 Service ID。ARXML 文件名不决定对象身份，必须同步更新内部 `SHORT-NAME` 和完整引用路径。

### 4.2 Required 服务实例

在 Machine1 的服务实例部署文件中新增：

```text
/SomeipInstancesDeployments/RequiredInstanceServiceHelloWorldClient2
```

它仍引用 `/SomeipDeployments/ServiceHelloWorld`，并使用：

```xml
<REQUIRED-MINOR-VERSION>0</REQUIRED-MINOR-VERSION>
<REQUIRED-SERVICE-INSTANCE-ID>29</REQUIRED-SERVICE-INSTANCE-ID>
<VERSION-DRIVEN-FIND-BEHAVIOR>MINIMUM-MINOR-VERSION</VERSION-DRIVEN-FIND-BEHAVIOR>
```

“Required 模型对象名称唯一”与“远端服务 Instance ID 相同”并不矛盾：两个客户端都要访问已有的实例 `29`。

### 4.3 进程及部署映射

在 `client2_deployment.arxml` 中配置：

| 关系 | 目标 |
| --- | --- |
| Process | `/Client2Deployment/client2_process` |
| Process → Design | `/Client2App/Client2Design` |
| Process → Executable | `/Client2App/exe/client2d` |
| Process → Machine | `/ISOFT/Development/Machine1` |
| 进程状态机类型 | `/ISOFT/ProcessModes/ProcessStateMachine` |
| ResourceGroup | `/ISOFT/Development/Machine1/OS/DefaultResourceGroup` |
| 启动配置 | `/Client2Deployment/startup_set/client2_config` |
| 启动功能组状态 | `HelloWorldGroup.Driving`、`HelloWorldGroup.Parking` |
| 根组件引用 | `/Client2App/exe/client2d/client2` |
| 目标端口引用 | `/Client2App/swc/client2/helloworld_RPort` |
| 服务实例 | 新增的 `RequiredInstanceServiceHelloWorldClient2` |
| 网络连接器 | `/ISOFT/Development/MachineDesign1/CommunicationConnector1` |

机器映射、端口映射、Find/Method Grant 中的服务实例引用都要指向新 Required 对象。

Grant 也必须使用唯一 FQN。本次使用 `/Grant/client2_findservicegrant` 和 `/Grant/client2_echomethodgrant`，避免与原客户端重名。

### 4.4 日志配置

在 Machine1 的 `machine_extension.arxml` 中增加 `helloworld_client2_log`，定义 `default`、`com`、`nai` 三个通道，分别使用 `#DFT`、`#COM`、`#NAI` Context ID。

本次 Application ID 为 `clt2`，启用控制台、文件和网络日志，文件目录为 `/var/redirected/`。在新进程部署中，将这三个通道映射到 `/Client2Deployment/client2_process`。

后续控制台日志中的 `clt2 #COM Info` 就来自这里。

### 4.5 软件集归属

复用 `CMDemo_client` 软件集，保留旧条目，在对应列表中分别增加：

```xml
<!-- CONTAINED-AR-ELEMENT-REFS 中 -->
<CONTAINED-AR-ELEMENT-REF DEST="EXECUTABLE">/Client2App/exe/client2d</CONTAINED-AR-ELEMENT-REF>

<!-- CONTAINED-PROCESS-REFS 中 -->
<CONTAINED-PROCESS-REF DEST="PROCESS">/Client2Deployment/client2_process</CONTAINED-PROCESS-REF>
```

同一 Process 不能重复归属多个软件集。保持 `.arxml.del` 文件禁用状态，不要误将旧服务端部署一并启用。

## 5. 校验模型并生成工程

先查询模型，确认引用和合并过程能通过：

```bash
"${ARAGEN}" --list-processes -o /tmp/client2-model-check \
  "${BASE_ARXMLS}" "${DEMO_DIR}/model"

"${ARAGEN}" --list-swcl-info -o /tmp/client2-model-check \
  "${BASE_ARXMLS}" "${DEMO_DIR}/model"
```

输出应包含 `client2_process`，软件集查询应将它关联到 `client2d`、`client2` SWC 和 Machine1。

首次创建工程时执行：

```bash
"${ARAGEN}" -g PROJECT \
  -e /Client2App/exe/client2d \
  -o "${DEMO_DIR}/client2" \
  "${BASE_ARXMLS}" "${DEMO_DIR}/model"
```

生成内容包括顶层与 `src/` 下的 CMake 文件、`src/main.cpp`、`files/aragen-helper.cmake`、`files/instance_specifier.txt` 和测试骨架。

生成的实例说明符为：

```text
client2d/client2/helloworld_RPort
```

已有业务代码后，不要直接重复执行 `-g PROJECT` 覆盖工程。需要更新模型衍生的 helper/实例说明时，可使用 `PROJECT_UPDATE` 并检查差异；当前生成器该分支不会重写业务 `main.cpp`。

## 6. 构建配置和客户端代码

### 6.1 接入 demo 构建

在 demo 顶层增加：

```cmake
add_subdirectory(client)
add_subdirectory(client2)
add_subdirectory(server)
```

在新工程的 `aragen_helper_add_build_helper_library()` 之前配置 ARXML 输入。当前实现已经补齐并去重，核心配置如下：

```cmake
set(ARXML_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/../model)
set(ARA_GEN_ARXMLS ${ARA_GEN_ARXMLS}
    ${ARA_ARXMLS_DIR}/common
    ${ARXML_PREFIX}/library_common
    ${ARXML_PREFIX}/app
    ${ARXML_PREFIX}/library_for_machine1_integration
    ${ARA_ARXMLS_DIR}/development_machines/system
    ${ARA_ARXMLS_DIR}/development_machines/machine1/common
)
aragen_helper_add_build_helper_library()
```

同一个模型目录只需列出一次，避免使用不同的相对写法重复加入。

生成的 `src/CMakeLists.txt` 将可执行程序链接到 `ARA_GEN_BUILD_HELPER_LIBRARY`，并调用 `aragen_helper_register_target()` 设置安装位置。不要遗漏这一步，否则部署工具可能找不到编译产物。

### 6.2 实现业务逻辑

参考原客户端 `client/src/main.cpp`，新客户端执行以下流程：

```text
Initialize → 注册 SIGTERM 处理 → 上报 kRunning
    → StartFindService → 创建 Proxy → 周期调用 EchoMethod
    → 收到退出信号后释放对象 → Deinitialize
```

复用生成接口：

```cpp
#include "hello/world/cm/servicehelloworld_proxy.h"
using Proxy = hello::world::cm::proxy::ServiceHelloWorldProxy;
```

本次将业务日志标记改为 `Helloworld-cm-Client2`，请求内容改为 `Com-Client2-Test[n]`。每隔约一秒调用一次 `EchoMethod()` 并打印返回的 `echo`。

当前代码沿用 `InstanceIdentifier::MakeAny()` 并选择首个发现的 handle。这次测试针对已有实例 `29`；若以后增加多个可选服务端，需要明确实例选择逻辑。后续已补齐 `testEvent` 订阅，修改点和验证见第 12 节。

### 6.3 编译并安装

```bash
"${SDK_ROOT}/build.sh" -i "${DEMO_DIR}"
```

`-i` 会执行安装，将程序放入部署工具的搜索目录。可以检查：

```bash
find "${HOME}/.isoft/tmp/ara_binout/Client2App" -type f -name client2d
```

路径结构为：

```text
~/.isoft/tmp/ara_binout/Client2App/exe/client2d/<SDK名称>/client2d
```

仅在 `.build` 下找到可执行文件，不代表已经完成安装或机器部署。

## 7. 配置 Machine1

确认平台已停止后再更新运行目录。若它正在运行，先执行 `sudo "${ARA_SYSROOT}/run.sh" -s`。

查看开发容器地址：

```bash
hostname -I
```

本次共享网络地址是 `172.18.0.2`。地址可能随容器重建变化；存在多个地址时，需要选择 `capi_shared_network` 对应的 IPv4 地址。

```bash
# 按实际地址修改，不要直接沿用过期 IP。
MACHINE1_IP="172.18.0.2"

"${SDK_ROOT}/config.sh" \
  -m Machine1 \
  -s "${ARA_SYSROOT}" \
  -a "${DEMO_DIR}" \
  -p "${MACHINE1_IP}"
```

本次输出包含 `Machine configuration updated successfully`。再检查部署结果：

```bash
ls "${ARA_SYSROOT}/ara/swcls/CMDemo_client/1.1.0/bin"
ls "${ARA_SYSROOT}/ara/swcls/CMDemo_client/1.1.0/etc"
```

实际看到：

```text
bin: client2d  clientd
etc: client2_process  client_process  function_groups.json
```

## 8. 启动双机

### 8.1 Machine2：服务端

先检查容器：

```bash
docker ps -a --filter name=capi_demo
```

已有容器处于停止状态时，本次使用以下命令恢复：

```bash
docker start capi_demo
```

首次建立 demo 环境时，则按原教程构建并启动：

```bash
docker compose -f "${CAPI_SRC_DIR}/demo/docker-compose.yml" build capi_demo
docker compose -f "${CAPI_SRC_DIR}/demo/docker-compose.yml" up -d capi_demo
```

容器启动命令会配置 Machine2 并启动平台。先观察初始化日志：

```bash
docker logs -f --since 1m capi_demo
```

在另一个终端、平台初始化完成后查询并切换功能组：

```bash
docker exec capi_demo sh -lc 'sudo "${ARA_SYSROOT}/run.sh" -C HelloWorldGroup'
docker exec capi_demo sh -lc 'sudo "${ARA_SYSROOT}/run.sh" -c HelloWorldGroup.Driving'
docker exec capi_demo sh -lc 'sudo "${ARA_SYSROOT}/run.sh" -C HelloWorldGroup'
docker exec capi_demo pgrep -a -x serverd
```

本次验证得到 `SetFgState Succeed: HelloWorldGroup.Driving`、`GetFgState Succeed: HelloWorldGroup.Driving`，并观察到 `serverd` 上报 `kRunning` 和 `OfferService` 成功。

单引号要保留，使 `${ARA_SYSROOT}` 在 Machine2 容器内部展开。本次没有改变服务接口，所以使用已有服务端镜像即可；修改服务端源码或模型后，需要重新构建并重建对应容器。

### 8.2 Machine1：客户端

终端 A 启动平台并保存控制台日志：

```bash
sudo "${ARA_SYSROOT}/run.sh" -R 2>&1 | tee /tmp/machine1-console.log
```

`-R` 表示不使用 cgroup 启动。保持此终端运行。

终端 B 在初始化完成后执行：

```bash
sudo "${ARA_SYSROOT}/run.sh" -c HelloWorldGroup.Driving
sudo "${ARA_SYSROOT}/run.sh" -C HelloWorldGroup
pgrep -af client2d
```

原客户端和新客户端共用软件集及功能组，所以都会启动。该功能组初始状态为 `Off`，只启动平台而不切换状态，不会触发这里配置的应用启动条件。

## 9. 查看日志与判断通信结果

### 9.1 新客户端日志

查看与新客户端相关的详细日志：

```bash
tail -f /tmp/machine1-console.log | grep --line-buffered -E 'Client2|client2|clt2'
```

只看业务 Info 日志：

```bash
tail -f /tmp/machine1-console.log | grep --line-buffered 'clt2 #COM Info'
```

日志中的关键阶段是：

```text
Helloworld-cm-Client2 ReportExecutionState kRunning
Helloworld-cm-Client2 StartFindService CB called
Helloworld-cm-Client2 [...] call EchoMethod recv echo: ... Com-Client2-Test[...]
```

### 9.2 服务端收到的请求

```bash
docker logs -f --since 1m capi_demo 2>&1 | grep --line-buffered 'Com-Client2-Test'
```

`/tmp/machine1-console.log` 是通过 `tee` 保存的文本输出。模型配置的 `/var/redirected/` 中也可能产生 `.dlt` 日志；不要将 DLT 文件默认当作普通文本用 `tail` 阅读。

### 9.3 本次实际测试证据

2026-09-14 的客户端日志包含以下连续调用，时间使用日志原样记录：

```text
08:11:07.075 ... clt2 #COM Info ... Client2 [135] ... Com-Client2-Test[136]
08:11:08.078 ... clt2 #COM Info ... Client2 [136] ... Com-Client2-Test[137]
08:11:09.080 ... clt2 #COM Info ... Client2 [137] ... Com-Client2-Test[138]
08:11:10.082 ... clt2 #COM Info ... Client2 [138] ... Com-Client2-Test[139]
08:11:11.084 ... clt2 #COM Info ... Client2 [139] ... Com-Client2-Test[140]
```

上述为便于阅读的日志节选，省略了固定前缀。完整返回内容包含 `From service iSOFT for CAPI:`。

较早的一次详细调用显示：客户端发送 `Com-Client2-Test[65]`，`OnResponse` 收到相同 session `65` 的响应，`_handleResponse` 成功解析回包。服务端日志也记录了 `Com-Client2-Test[12]`、`[13]` 等请求和 `rcode: 0` 的正常响应。

| 验证点 | 本次结果 |
| --- | --- |
| ARXML 语法 | demo 的 15 个 ARXML 通过解析 |
| 软件集关系 | 正确识别新 Process、Executable、SWC 和 Machine1 |
| 配置生成 | 临时生成验证中的 32 个 JSON 均可解析 |
| 工程生成 | `PROJECT` 成功，实例说明符正确 |
| 编译安装 | 用户执行成功，安装目录中存在 `client2d` |
| Machine1 部署 | 程序和 `client2_process` 配置已进入运行目录 |
| Machine2 服务 | `Driving`、`serverd`、`OfferService` 已确认 |
| 方法通信 | 新客户端持续收到 EchoMethod 回包，所示 5 次调用间隔约一秒 |
| 事件通信 | 初次方法通信测试未验证；后续订阅测试见第 12 节 |
| 压力、重连、长时间稳定性 | 本次未验证 |

`Client2 [135]` 与消息中的 `[136]` 相差一，是因为业务日志打印 `nLoopCount`，请求内容使用 `nLoopCount + 1`。这不是丢包或服务端篡改序号。

## 10. 本次遇到的问题及修复

| 现象或问题 | 原因 | 修复 |
| --- | --- | --- |
| `Opening and ending tag mismatch` | 服务实例 ARXML 多了一个 `</ELEMENTS>` | 删除多余闭合标签 |
| ARXML 合并报 `CODE-003`、Grant 已存在 | 新旧客户端使用相同 Grant FQN | 新 Grant 改为 `client2_findservicegrant`、`client2_echomethodgrant` |
| 新根组件仍引用旧 SWC | 复制设计文件时漏改引用 | 改为 `/Client2App/swc/client2` |
| 日志映射引用 `/ClientDeployment/client2_process` | 新 Process 位于 `Client2Deployment` 包 | 修正完整 Process 引用 |
| 引用 `ProcessStateMachine1` | 模型中没有这个状态机类型 | 复用已有 `ProcessStateMachine` |
| `helloworld_client2_log` 引用不存在 | 只新增了引用，没有定义日志通道 | 在 Machine1 扩展中补齐三条通道 |
| 新 Required 实例没有接入 | 部署和 Grant 仍指向原 Required 实例 | 将相关引用统一到新 Required 对象 |
| 编译成功但运行目录无 client2d | 尚未执行机器配置 | 编译安装后执行 `config.sh` |
| `container ... is not running` | `docker exec` 不会启动容器 | 先 `docker start capi_demo`，初始化后再切功能组 |

如果后续没有回包，按顺序检查：

1. 没有 `kRunning`：检查部署结果、进程和功能组状态。
2. 有 `kRunning`，没有发现服务回调：检查服务端 Offer、实例匹配、共享网络及 IP。
3. 已发现服务但方法不返回：查看两端完整日志，核对方法 ID、请求和响应 session。

复制本文代码块时，下划线前不要添加反斜杠；使用 `${ARA_SYSROOT}`，不要写成 `${ARA\_SYSROOT}`。Bash 的 `>` 是等待续行的提示符，不是需要复制的命令内容。

## 11. 停止测试

在独立日志终端对 `tail | grep` 或 `docker logs -f` 按 `Ctrl+C`，只会停止日志查看。不要把它与运行平台的终端 A 混淆。

正常停止两端平台：

```bash
sudo "${ARA_SYSROOT}/run.sh" -s
docker exec capi_demo sh -lc 'sudo "${ARA_SYSROOT}/run.sh" -s'
```

再次测试时先检查容器状态；Machine2 平台停止后容器可能退出，需要重新启动容器。每次更新模型或程序，都应重新完成对应的编译安装、机器配置和启动验证。

## 12. 补齐 client2 的事件订阅

本次复用已有 `testEvent`，无需新增服务接口或修改 server。服务端每处理一次 `EchoMethod` 就发送一个 ByteArray，内容为 `abc`；事件组 `Eventgroup1` 已在 client2 的 Required 实例中配置。

修改位置（相对于 `samples/helloworld-cm`）：

| 文件 | 修改内容 |
| --- | --- |
| `client2/src/main.cpp` | 创建 Proxy 后注册订阅状态和接收回调，调用 `testEvent.Subscribe(8)`；接收回调通过 `GetNewSamples` 读取并打印数据，保留周期 EchoMethod 调用 |
| `model/library_for_machine1_integration/client2_deployment.arxml` | 新增 `/Grant/client2_testeventgrant`，绑定 client2 Required 实例与 `testEvent` 部署 |
| `model/library_for_machine1_integration/machine_extension.arxml` | 在 Machine1 的 `IamModuleInstantiation` 中引用 client2 的发现、方法、事件 Grant |

`Subscribe(8)` 中的 8 是样本容量，不是发送周期。订阅请求返回成功后，仍需观察异步状态是否进入 `Subscribed`。退出时先等待正在执行的回调结束、禁止后续回调访问对象，然后取消订阅并移除回调，最后释放 Proxy 和反初始化平台。

Machine1 基础模型的本地及远程访问控制开关仍为 `false`。此次补齐了 Grant 及其引用，但没有验证启用 IAM 后的权限拦截行为。

重新执行第 6.3 节编译安装、第 7 节机器配置，以及第 8 节双机启动后，在开发容器中观察：

```bash
tail -f /tmp/machine1-console.log | grep --line-buffered -E 'Client2 testEvent|Helloworld-cm-Client2.*recv echo'
```

2026-09-14 后续双机实测日志（空白已归一化）：

```text
08:35:48.266 ... clt2 #COM Info [ Client2 testEvent subscription: SubscriptionPending ]
08:35:48.268 ... clt2 #COM Info [ Client2 testEvent subscription: Subscribed ]
08:35:48.268 ... clt2 #COM Info [ Client2 testEvent received: abc bytes: 3 ]
08:35:49.271 ... clt2 #COM Info [ Client2 testEvent received: abc bytes: 3 ]
```

此次验证使用独立日志 `/tmp/client2-event-machine1.log`，避免覆盖初次测试的 `/tmp/machine1-console.log`。检查时已收到 40 条 `abc` 事件与 20 条 client2 EchoMethod 回包。原客户端和 client2 都会触发 server 发送事件，因此事件条数不必与 client2 的方法调用次数相等；事件 payload 也不携带请求序号。

若只有 EchoMethod 回包却没有事件，先检查订阅状态，再检查两端的事件 ID、事件组、Required 实例和生成配置。`SubscriptionPending` 表示尚未完成订阅，不能作为接收成功的证据。
