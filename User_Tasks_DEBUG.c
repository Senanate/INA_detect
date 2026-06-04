/* 
 * 按键蜂鸣器调试版本
 * 
 * 使用方法：
 * 1. 备份原 User/Tasks/Src/User_Tasks.c
 * 2. 将此文件内容替换到 User_Tasks.c
 * 3. 编译烧录，观察现象
 * 4. 根据现象判断问题原因
 */

#include "Task_Init.h"
#include "key.h"
#include "beep.h"

/* ============================================================================
 * 调试配置开关
 * ============================================================================ */
#define DEBUG_STARTUP_BEEP      1    // 启动自检：蜂鸣器响3次
#define DEBUG_GPIO_DIRECT       1    // 直接读GPIO绕过Key_Scan
#define DEBUG_LED_HEARTBEAT     0    // LED心跳指示任务运行（需要LED）
#define DEBUG_USE_KEY_SCAN      1    // 使用原Key_Scan逻辑

/* ============================================================================
 * KeyTask - 按键任务（调试版本）
 * ============================================================================ */
void KeyTask(void *argument)
{
    key_event_t key_event;
    
#if DEBUG_LED_HEARTBEAT
    uint32_t heartbeat_counter = 0;
#endif

#if DEBUG_GPIO_DIRECT
    uint8_t key_down_flag = 0;
#endif
    
    /* ========== 启动自检 ========== */
#if DEBUG_STARTUP_BEEP
    // 等待系统稳定
    osDelay(1000);
    
    // 蜂鸣器响3次，每次100ms
    for(int i=0; i<3; i++){
        beep_open();
        osDelay(100);
        beep_close();
        osDelay(200);
    }
    
    // 如果听到3次短促的"滴滴滴"：
    //   ✅ PWM硬件配置正常
    //   ✅ beep_open/close函数工作正常
    //   ✅ 蜂鸣器硬件正常
    //
    // 如果没有声音：
    //   ❌ PWM配置有问题，检查tim.c中TIM4配置
    //   ❌ 蜂鸣器硬件有问题
    
    osDelay(1000);
#endif
    /* ========== 自检结束 ========== */
    
    while(1)
    {
        /* ========== LED心跳（任务存活指示）========== */
#if DEBUG_LED_HEARTBEAT
        // 每1秒翻转一次LED（假设PC13是LED）
        if(++heartbeat_counter >= 100){  // 10ms * 100 = 1秒
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            heartbeat_counter = 0;
        }
        
        // 如果LED闪烁：
        //   ✅ FreeRTOS任务正常运行
        //   ✅ osDelay工作正常
        //
        // 如果LED不闪：
        //   ❌ 任务未被调度或死机
#endif
        
        /* ========== 直接读GPIO（绕过Key_Scan）========== */
#if DEBUG_GPIO_DIRECT
        // 直接读取KEYB（PA0）的状态
        // 按键按下时为低电平（GPIO_PIN_RESET）
        if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET)
        {
            // 按键按下
            if(!key_down_flag){
                beep_open();
                key_down_flag = 1;
            }
        }
        else
        {
            // 按键释放
            if(key_down_flag){
                beep_close();
                key_down_flag = 0;
            }
        }
        
        // 如果按PA0时蜂鸣器响：
        //   ✅ 按键硬件正常
        //   ✅ GPIO配置正常
        //   ⚠️ 如果Key_Scan不响，说明Key_Scan有问题
        //
        // 如果按PA0蜂鸣器不响：
        //   ❌ 按键硬件未连接或连接错误
        //   ❌ GPIO配置有问题
#endif
        
        /* ========== 原有Key_Scan逻辑 ========== */
#if DEBUG_USE_KEY_SCAN
        if(Key_Scan(&key_event)){
            /* 发送按键事件到消息队列 */
            osMessageQueuePut(Key_MessageQueue, &key_event, 0, 0);
            
            /* 按键控制蜂鸣器 */
            if(key_event.type == KEY_EVT_DOWN){
                beep_open();
            }else if(key_event.type == KEY_EVT_UP){
                beep_close();
            }
        }
        
        // 如果Key_Scan能让蜂鸣器响：
        //   ✅ 所有功能正常
        //
        // 如果Key_Scan不响但直接读GPIO能响：
        //   ❌ Key_Scan()函数有问题
        //   ❌ Key_Init()可能未正确执行
#endif
        
        osDelay(10);
    }
}

/* ============================================================================
 * 调试信息总结
 * ============================================================================
 * 
 * 现象1: 启动时响3次，按PA0蜂鸣器响，按其他键也响
 *   ✅ 完全正常！可以关闭所有调试开关，恢复原代码
 * 
 * 现象2: 启动时响3次，按PA0蜂鸣器响，按其他键不响
 *   ✅ PWM正常，KEYB正常
 *   ⚠️ 检查其他按键硬件连接
 * 
 * 现象3: 启动时响3次，按PA0不响
 *   ✅ PWM正常
 *   ❌ KEYB按键硬件有问题或GPIO配置冲突
 *   🛠️ 解决方案：
 *      - 检查PA0硬件连接
 *      - 注释掉 key.c 中 Key_Port_Init() 的GPIO配置
 * 
 * 现象4: 启动时不响，按键也不响
 *   ❌ PWM配置有严重问题
 *   🛠️ 解决方案：
 *      1. 检查 tim.c 中 MX_TIM4_Init() 是否被调用
 *      2. 检查 tim.c 中 HAL_TIM_MspPostInit(&htim4) 是否被调用
 *      3. 使用示波器测量PB9引脚，看是否有PWM波形
 *      4. 检查蜂鸣器电源供电
 * 
 * 现象5: 启动时响3次，直接读GPIO能响，但Key_Scan不响
 *   ✅ 硬件全部正常
 *   ❌ Key_Scan()函数有问题
 *   🛠️ 解决方案：
 *      - 检查 Key_Init() 是否在 Hardware_InitTask 中被调用
 *      - 检查 key.c 中 Key_RegisterDefaultKeys() 是否正确注册了按键
 *      - 在 Key_Scan() 中添加调试代码，看是否能检测到按键变化
 * 
 * ============================================================================ */
