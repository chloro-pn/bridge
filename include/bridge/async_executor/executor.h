#pragma once

#include "async_simple/Executor.h"

namespace bridge {
/*
 * 使用async_simple实现多核并行的编码和解码工作
 * 思路：当某个协程在编码/解码一个Array或者Map时，可以选择性的将Array或Map拆分成1-多个子任务
 * 然后调用collectAll挂起自身并等待子任务完成，继续解析剩余部分
 */
class BridgeExecutor : public async_simple::Executor {

};
}