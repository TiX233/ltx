# ltx 裸机调度器 V3

目录：

- [ltx 裸机调度器 V3](#ltx-裸机调度器-v3)
  - [一、项目简介](#一项目简介)
  - [二、基础组件](#二基础组件)
    - [1、闹钟/Alarm](#1闹钟alarm)
    - [2、话题/Topic](#2话题topic)
  - [三、拓展组件](#三拓展组件)
    - [1、app/应用](#1app应用)
    - [2、script/脚本](#2script脚本)
    - [3、debug/调试](#3debug调试)
    - [4、lock/锁](#4lock锁)
    - [5、event\_group/事件组](#5event_group事件组)
  - [四、移植](#四移植)
  - [五、空闲休眠](#五空闲休眠)
  - [六、tickless](#六tickless)

## 一、项目简介

这是一个轻量级的事件驱动裸机调度器，支持空闲任务，更方便空闲休眠，支持 tickless，目前仅在几个个人项目中使用，具体如下：

ltx V1:

* [我的世界红石_数字方案](https://oshwhub.com/realtix/minecraft_redstone_digital)
* [反应力测试器](https://oshwhub.com/realtix/human_benchmark)
* [我的世界米家拉杆](https://oshwhub.com/realtix/lever)
* [歌曲封面显示唱片机](https://oshwhub.com/realtix/cover_display)

ltx V2:

* [裸机空闲休眠时钟（见 git 历史提交）](https://oshwhub.com/realtix/idle_sleep_clock)

ltx V3:

* [裸机空闲休眠时钟](https://oshwhub.com/realtix/idle_sleep_clock)

ltx V1 由软件多定时器发展而来，通过加入闹钟和话题组件来提供事件驱动能力，但是它与大多数裸机调度器一样，调度器需要在主循环不断遍历活跃组件的事件链表，时间复杂度为 O(n)，不仅响应性会降低，还无法提供高效的空闲休眠能力。  

ltx V2 对 V1 进行了重构。V2 的事件产生不再是将活跃事件置位，而是将话题指针推入话题就绪链表队列，主循环不断弹出就绪话题并调用其回调即可，效率优化至 O(1)。事件响应性更高且可以更快判断系统是否空闲。  
虽然理论上可以在就绪任务判空后进入休眠，不过 V2 为了便于相关操作，设计了一个空闲任务功能。如果需要空闲任务/空闲休眠功能，则可将调度函数放在一个最低优先级的软中断里而非主函数，并在 `ltx_config.h` 做一些基础配置，然后在主函数中调用空闲任务即可。

ltx V3 对 V2 的闹钟部分进行了重构，删除了软件定时器，内建 tickless 支持。在 V2 中，闹钟活跃双链表的增删都为 O(1)，但是 systick 中进行遍历还是 O(n)，而且也并不方便写 tickless，因为要遍历去寻找最近触发的闹钟，所以 V3 将闹钟改为顺序存储，每个闹钟存储自身与前一个闹钟的时间差，那么 systick 只需弹出首(几)个闹钟节点即可，且进行 tickless 操作时只需要获取首个节点的倒计时即可，效率更高。

## 二、基础组件

### 1、闹钟/Alarm

闹钟组件常用于某些事件的超时检查，最初设计用于中断按键消抖，例如某个按键中断发生后立即创建一个 20ms 后的闹钟，每次抖动都会将闹钟倒计时重置为 20ms，直到不抖动 20ms 后闹钟到时调用回调，通过设置标志位，可区分第一次边沿变化与后续抖动，提高中断按键的响应速度以及提供更优雅的消抖方案，例如 [反应力测试器项目](https://gitee.com/TiX233/reaction_tester) 可提供仅 1us 延迟的中断按键功能

ltx V2 已为闹钟分配话题而非直接调用闹钟回调

ltx V3 删除了软件定时器，如果需要定期执行一些任务，那么可以在闹钟订阅者回调中设置下次闹钟的时间或者直接使用拓展组件中的 task/周期任务 组件

### 2、话题/Topic

话题组件的发布订阅机制用于提供事件驱动能力。与其他裸机框架不同，话题并不依附于任务而是独立存在。

最初设计用于数据发布，例如设置一个 20ms 一次的发起 adc dma 采样的周期任务，那么在 dma 采样完成回调里发布数据更新话题即可让所有订阅者获取最新的数据，无需所有数据使用者均创建一个 20ms 的周期任务来 poll 数据，并且通过订阅与取消订阅，可方便地对数据打印进行开关。闹钟到时也是通过发布话题通知订阅者实现的，所以可以实现对某些定时器任务进行 hook 操作

## 三、拓展组件

ltx 提供的 `app`、`脚本`、`事件组` 等等额外拓展功能组件皆由上述三个基础组件组合而来。  

### 1、app/应用

通过对软件定时器的封装，抽象出周期任务 task 以及隔离各业务的 app。

app 与 task 都有四个运行状态变更接口，分别为 `初始化`、`暂停`、`继续` 以及 `销毁`。

例如可以将采集传感器的内容放在一个 app 内，创建多个 task 来定期采集数据，那么外部就可以较为分明地对其进行启停。在 `ltx_cmd.c` 中有一个可供用户调试 app 的 `ltx_app` 命令，通过这个命令，可以用串口来打印所有 app 及其 task，并对其进行启停操作，便于调试

### 2、script/脚本

脚本组件原设计用于执行特定的序列如显示屏初始化这种需要多次发送数据的固定步骤场景，  
现已丰富为类似状态机协程的存在，配合 ltx 调度器，可实现步骤间非阻塞延时以及新步骤非阻塞等待话题发布并设置等待超时时间，  
大部分场景可取代 app 组件提供的 task 这一定时轮询任务功能，  
通过设置步骤间延时为 0 tick，可实现仅出让单次调度轮询，而非出让一整个时间片（1 ms）

### 3、debug/调试

内含两个部分，分别为 `ltx_cmd.c` 和 `ltx_param.c`，前者用于创建一些串口调试命令，后者用于创建一些可供串口修改的系统参数。

通过自定义命令，可控制单片机的运行状态，比如暂停某些 app 等，也可依赖发布订阅机制实现数据更新后的自动打印，在 `ltx_cmd.c` 中提供的 `/print` 命令有一个 `heart_beat` 样例，用来每秒打印心跳，您可参考该样例来设置自己的订阅数据打印；  
如果您需要经常修改一些参数如尝试某些不同的背景颜色，那么也无需重新烧录，在 `ltx_cmd.c` 中提供了一个 `/param` 命令，该命令可更改 `ltx_param.c` 中指向的自定义数据进行读写；

所有的自定义命令可在 `ltx_cmd.c` 中查看，也可开机后给单片机发送 `/help` 命令来列出所有命令，您也可以参考这些命令创建一些方便调试自定义命令。

### 4、lock/锁

用于对某些资源进行上锁操作，例如 spi 正在 dma 刷屏，则可在刷屏前上锁，在发送完成回调中解锁。  
锁有超时机制，当锁超时后，会调用用户自定义超时回调，用户可以在这里解锁资源与锁，例如强制停止 dma 发送。  
锁在解锁时会发布一个话题。

锁机制对于裸机意义不大，但是可以作为一个能保存标志位的 topic  

目前常用于按键消抖，或者作为一个配置更简单的闹钟，因为 V2 及之后的版本的闹钟配起来很麻烦，而且锁有超时回调

### 5、event_group/事件组

可设置等待超时时间以及 31 个等待的事件，设计用于等待所有硬件初始化完成，使用一个 uint32 的整数来存储所有事件，最高位用于判断事件组回调是否为超时所调用

## 四、移植

如果仅需要基础组件，那么只需要移植 `ltx.c` 和 `ltx.h` 即可。

因为是裸机调度器，所以移植几乎没有难度，最快仅需三步即可：

1、配置开关中断宏

在 `ltx_config.h` 中选择对应的硬件架构，也就是解除所用的架构的头文件注释：

```c
// 选择一个对应架构的配置文件
#include "ltx_arch_arm_cortex_m.h"
// #include "ltx_arch_xxx.h"
```

如果使用的是暂时没有支持的架构，那么只需要根据你的环境修改相应的开关中断宏：

```c
#define _LTX_IRQ_ENABLE()           __enable_irq()
#define _LTX_IRQ_DISABLE()          __disable_irq()
```

2、系统嘀嗒

在 1ms 周期的 systick 中断中调用如下函数：

```c
ltx_Sys_tick_tack();
```

3、在 main.c 最后调用如下函数：


```c
ltx_Sys_scheduler();
```

**例如：**

```c
#include "main.h"
#include "ltx.h"
// #include "myAPP_system.h"
// #include "myAPP_device_init.h"

// ...

int main(void){
    // 初始化外设
    // ...

    // 创建组件
    // ...

    /* 如果想拥有更强的业务隔离与管理能力，那么还可以额外引入 ltx_app 组件
    // 创建 app
    // 系统调试 app
    ltx_App_init(&app_system); // 初始化 app
    ltx_App_resume(&app_system); // 运行 app

    // 硬件初始化 app
    ltx_App_init(&app_device_init); // 初始化 app
    ltx_App_resume(&app_device_init); // 运行 app
    */

    // 运行调度器
    ltx_Sys_scheduler(); // 调度器内部有一个无限循环，所以后续代码不会被运行

    while(1);
}

// ...

// systick 中断服务函数
void SysTick_Handler(void)
{
    HAL_IncTick();

    // 添加系统嘀嗒
    ltx_Sys_tick_tack();
}
```

## 五、空闲休眠

一般在 rtos 中，都会提供一个最低优先级的 idle 任务，用户可以在这里将系统设置为休眠并等待中断唤醒。

以下是一般裸机实现的伪代码：  

```c
int main(void){
    // 调度器无限循环
    while(1){
        __disable_irq();
        task = get_active_task();
        if(task != NULL){ // 有任务需要执行
            __enable_irq();
            task->callback(task);
        }else { // 空闲
            // 虽然关了中断，但是只是不能进中断服务函数，还是能唤醒设备
            __WFI(); // 进入空闲休眠，如果有就绪的中断也会直接退出 wfi
            // 开中断必须放在 wfi 后面，不然会产生空窗期导致丢失事件响应
            __enable_irq();
        }
    }
}
```

一般的裸机调度器在 `get_active_task()` 的时候都是遍历所有任务的事件，时间复杂度会来到 O(N)，ltx V1 也是如此，所以会导致响应性变差以及判空效率低，所以 V2 及后续版本的发布事件的时候不再是置位标志位，而是将话题指针推入就绪队列，将获取活跃任务改为了弹出队列，也就是时间复杂度为 O(1)，效率更高。  

不仅如此，为了方便操作，V2 及后续版本可以以抬升优先级的方式，实现一个真正的空闲任务。也就是将调度器跑在一个最低优先级的软中断里（例如 pendsv），此时主循环就是最低优先级的空闲任务。例程如下：

```c
#include "main.h"
#include "ltx.h"
// #include "myAPP_system.h"
// #include "myAPP_device_init.h"

// ...

int main(void){
    // 初始化外设
    // ...

    // 创建组件
    // ...

    /* 如果想拥有更强的业务隔离与管理能力，那么还可以额外引入 ltx_app 组件
    // 创建 app
    // 系统调试 app
    ltx_App_init(&app_system); // 初始化 app
    ltx_App_resume(&app_system); // 运行 app

    // 硬件初始化 app
    ltx_App_init(&app_device_init); // 初始化 app
    ltx_App_resume(&app_device_init); // 运行 app
    */

    // 这里不再直接调用调度器了，而是直接运行空闲任务
    ltx_Sys_idle_task(); // 空闲任务内部有一个无限循环，所以后续代码不会被运行

    while(1);
}

// ...

// systick 中断服务函数
void SysTick_Handler(void)
{
    HAL_IncTick();

    // 添加系统嘀嗒
    ltx_Sys_tick_tack();
}

void PendSV_handler(void){
    // 在这里运行调度器，但是需要对 ltx_config.h 做一些修改，不然空闲任务无法运行
    ltx_Sys_scheduler();
}

void ltx_Sys_idle_task(void){
    while(1){
        // 直接进入休眠，等待中断唤醒
        __WFI();
    }
}
```

如果使用的是目前已支持的架构，那么只需要在 `ltx_config.h` 中解除相关头文件的注释即可：

```c
// 选择一个对应架构的配置文件
#include "ltx_arch_arm_cortex_m.h"
// #include "ltx_arch_xxx.h"
```

然后打开 `ltx_config.h` 中的空闲任务开关宏（取消其注释）：

```c
// 需要空闲任务则打开此宏，并将调度器从主循环转移到最低优先级的软中断中
#define ltx_cfg_USE_IDLE_TASK
// 需要 tickless 则打开此宏，前提是必须打开空闲任务宏
// #define ltx_cfg_USE_TICKLESS
```

这种处理已经非常接近 rtos 了，不过 rtos 里面 PendSV 只承担切换上下文作用，不运行任务，而 ltx V2+ 则是在这里弹出任务并运行，仅为了抬升优先级。

## 六、tickless

即使系统进入了空闲休眠，还是会每个 tick 被 systick 中断唤醒然后发现没有事要做就又休眠了。理论上可以通过计算阻塞任务的延时时间来将 systick 中断推迟，减少 systick 对空闲休眠的打扰，也就是 tickless。  

V3 重构原有机制，也就是不按照传统裸机的每毫秒进一次 systick 去递减 tick 计数值。而是动态修改 systick 的周期，每次设置闹钟的时候，增加一个判断，如果这个时间比当前 systick 倒计时还要快，那么就提前 systick 到来，否则保持。这样也就不需要空闲后计算下个任务的到来时间，而是保持只有一条 `__WFI()`。  
原理上倒是和一些 rtos 差不多，使用时只要配置相应架构的 systick 操作宏，然后打开 `ltx_config.h` 中的空闲任务开关宏（取消其注释）：

```c
// 需要空闲任务则打开此宏，并将调度器从主循环转移到最低优先级的软中断中
#define ltx_cfg_USE_IDLE_TASK
// 需要 tickless 则打开此宏，前提是必须打开空闲任务宏
#define ltx_cfg_USE_TICKLESS
```

可以在空闲钩子函数中执行一些低功耗出入操作
