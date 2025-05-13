#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_saadc.h"
#include "app_timer.h"
#include "ble_advdata.h"
#include "ble_srv_common.h"
#include "ble_bas.h"
#include "ble_conn_params.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "app_error.h"
#include "ble_temp_service.h"

#define ADC_INPUT NRF_SAADC_INPUT_AIN0
#define APP_BLE_CONN_CFG_TAG 1
#define DEVICE_NAME "BLE-NTC"
#define TEMP_MEAS_INTERVAL APP_TIMER_TICKS(5000)

BLE_TEMP_SERVICE_DEF(m_temp_service);
APP_TIMER_DEF(m_temp_timer_id);

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;

void saadc_init(void)
{
    nrf_saadc_channel_config_t config = NRF_SAADC_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN0);
    nrf_saadc_enable();
    nrf_saadc_channel_init(0, &config);
}

float adc_to_temp(int16_t adc_value)
{
    float Vref = 3.3;
    float R_fixed = 10000.0;
    float Vadc = ((float)adc_value / 1023.0) * Vref;

    float R_ntc = (Vref * R_fixed / Vadc) - R_fixed;

    float B = 3977.0;
    float T0 = 298.15; // 25°C in Kelvin
    float R0 = 10000.0;

    float temp_K = 1.0 / (1.0 / T0 + (1.0 / B) * log(R_ntc / R0));
    return temp_K - 273.15; // Return °C
}

void temp_timer_handler(void *p_context)
{
    nrf_saadc_value_t adc_val;
    nrf_saadc_sample_convert(0, &adc_val);

    float temp = adc_to_temp(adc_val);
    int16_t temp_int = (int16_t)(temp * 100); // GATT format: int16_t x100

    ble_temp_service_temperature_update(&m_temp_service, temp_int);
}

void timers_init(void)
{
    app_timer_init();
    app_timer_create(&m_temp_timer_id, APP_TIMER_MODE_REPEATED, temp_timer_handler);
}

void ble_stack_init(void)
{
    nrf_sdh_enable_request();
    uint32_t ram_start = 0;
    nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    nrf_sdh_ble_enable(&ram_start);
}

void gap_params_init(void)
{
    ble_gap_conn_params_t conn_params = {0};
    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *)DEVICE_NAME, strlen(DEVICE_NAME));
    sd_ble_gap_ppcp_set(&conn_params);
}

void services_init(void)
{
    ble_temp_service_init(&m_temp_service);
}

void advertising_start(void)
{
    // Init advertising here (not shown for brevity)
}

int main(void)
{
    saadc_init();
    timers_init();
    ble_stack_init();
    gap_params_init();
    services_init();
    advertising_start();

    app_timer_start(m_temp_timer_id, TEMP_MEAS_INTERVAL, NULL);

    while (true)
    {
        nrf_pwr_mgmt_run();
    }
}
