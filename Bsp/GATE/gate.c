#include "gate.h"
#include "math.h"

static uint8_t gate_status = HIGH_CUR; // 当前 GPIO 流路对应的实际挡位
static uint8_t gate_mode = GATE_MODE_AUTO; // 当前使用自动判挡或固定挡位策略

void Gate_Port_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(EN1_PORT, EN1_PIN, GPIO_PIN_SET);
	HAL_GPIO_WritePin(EN2_PORT, EN2_PIN, GPIO_PIN_RESET);

  /*Configure GPIO pin : PB3 */
  GPIO_InitStruct.Pin = EN1_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(EN1_PORT, &GPIO_InitStruct);

	/*Configure GPIO pin : PB3 */
  GPIO_InitStruct.Pin = EN2_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(EN2_PORT, &GPIO_InitStruct);
}

/* 两个控制脚分属不同 GPIO 端口。HIGH 切到 MID 时，当前写入顺序会先经过 LOW 的控制组合。 */
void flow_route_selection(uint8_t selection)
{
  if(selection > HIGH_CUR)
    selection = HIGH_CUR; // Invalid selection
  if(selection < LOW_CUR)
    selection = LOW_CUR;
	if(selection == HIGH_CUR)
  {
    HAL_GPIO_WritePin(EN1_PORT, EN1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(EN2_PORT, EN2_PIN, GPIO_PIN_RESET);
  }
  else if(selection == MID_CUR)
  {
    HAL_GPIO_WritePin(EN1_PORT, EN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(EN2_PORT, EN2_PIN, GPIO_PIN_SET);
  }
  else if(selection == LOW_CUR)
  {
    HAL_GPIO_WritePin(EN1_PORT, EN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(EN2_PORT, EN2_PIN, GPIO_PIN_RESET);
  }
  gate_status = selection; // Update the status
}

// 获取当前档位状态
uint8_t Gate_get_status(void)
{
  return gate_status;
}

/* AUTO 分支仅保存模式，由 ADCTask 根据采样判挡。固定挡位分支会立即写 GPIO，
 * 运行中切换流路需要与 ADCTask 的暂停采样和清队列过程同步。
 */
void Gate_Set_Mode(uint8_t mode)
{
  if (mode == GATE_MODE_AUTO) {
    gate_mode = GATE_MODE_AUTO;
    return;
  }

  if (mode == GATE_MODE_LOW || mode == GATE_MODE_MID || mode == GATE_MODE_HIGH) {
    gate_mode = mode;
    flow_route_selection(mode);
  }
}

// 获取当前档位模式
uint8_t Gate_Get_Mode(void)
{
  return gate_mode;
}

