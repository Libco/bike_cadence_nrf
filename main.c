/**
 * Copyright (c) 2014 - 2019, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/** @file
 *
 * @defgroup ble_sdk_app_csc_main main.c
 * @{
 * @ingroup ble_sdk_app_csc
 * @brief Cycling Speed and Cadence Service Sample Application main file.
 *
 * This file contains the source code for a sample application using the Cycling Speed and Cadence
 * Service.
 * It also includes the sample code for Battery and Device Information services.
 * This application uses the @ref srvlib_conn_params module.
 *
 * This application implements supports for both Wheel revolution Data and Crank Revolution Data.
 * In addition, this application also has support for all 'Speed and Cadence Control Point'.
 */
#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "ble.h"
#include "ble_err.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_bas.h"
#include "ble_cscs.h"
#include "ble_dis.h"
#include "ble_conn_params.h"
//#include "sensorsim.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "app_timer.h"
#include "peer_manager.h"
#include "peer_manager_handler.h"
#include "bsp_btn_ble.h"
#include "fds.h"
#include "ble_conn_state.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_pwr_mgmt.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "SEGGER_RTT.h"

//
#include "boards.h"
#include "app_util_platform.h"
#include "app_error.h"
#include "nrf_drv_twi.h"
#include "nrf_delay.h"

//GPIO
#include "nrf_gpio.h"

/* TWI instance ID. */
#define TWI_INSTANCE_ID 0

#define TIMEOUT_TO_SLEEP 120000
#define ACC_SENSITIVITY 2000

/* Common addresses definition for acc/gyrp/temperature sensor. */
#define MPU6050_ADDR 0x68

/* Indicates if operation on TWI has ended. */
static volatile bool m_xfer_done = false;

/* TWI instance. */
static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID);

/* Buffer for samples read from sensor. */
static uint8_t m_sample[14];

//
static volatile double currentRotation = 0;
static volatile bool rotationRead = false;

static volatile bool CrankRevsChange = false;
static volatile uint16_t CrankRevs = 0;
static volatile uint16_t NoChangeForMS = 0;

static volatile bool IsGyroInitialized = false;
static volatile bool IsGyroTimerRunning = false;
static volatile bool IsIdle = true;

///
int16_t accXPrev = 0;
int16_t accYPrev = 0;
int16_t accZPrev = 0;

///////////////

bool isBleAdvertisingOn = false;

//////////////

#define DEVICE_NAME "Cadence_MKI"                /**< Name of device. Will be included in the advertising data. */
#define MANUFACTURER_NAME "NordicSemiconductor" /**< Manufacturer. Will be passed to Device Information Service. */

#define APP_BLE_OBSERVER_PRIO 3 /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define APP_BLE_CONN_CFG_TAG 1  /**< A tag identifying the SoftDevice BLE configuration. */

#define APP_ADV_INTERVAL 40 /**< The advertising interval (in units of 0.625 ms. This value corresponds to 25 ms). */

#define APP_ADV_DURATION 18000 /**< The advertising duration (180 seconds) in units of 10 milliseconds. */

#define ACCELEROMETER_CHECK_INTERVAL APP_TIMER_TICKS(2000) /**< Battery level measurement interval (ticks). */
#define MIN_BATTERY_LEVEL 81                               /**< Minimum battery level as returned by the simulated measurement function. */
#define MAX_BATTERY_LEVEL 100                              /**< Maximum battery level as returned by the simulated measurement function. */
#define BATTERY_LEVEL_INCREMENT 1                          /**< Value by which the battery level is incremented/decremented for each call to the simulated measurement function. */

#define SPEED_AND_CADENCE_MEAS_INTERVAL 10 /**< Speed and cadence measurement interval (milliseconds). */

#define MIN_CONN_INTERVAL MSEC_TO_UNITS(500, UNIT_1_25_MS)   /**< Minimum acceptable connection interval (0.5 seconds). */
#define MAX_CONN_INTERVAL MSEC_TO_UNITS(1000, UNIT_1_25_MS)  /**< Maximum acceptable connection interval (1 second). */
#define SLAVE_LATENCY 0                                      /**< Slave latency. */
#define CONN_SUP_TIMEOUT MSEC_TO_UNITS(4000, UNIT_10_MS)     /**< Connection supervisory timeout (4 seconds). */
#define FIRST_CONN_PARAMS_UPDATE_DELAY APP_TIMER_TICKS(5000) /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY APP_TIMER_TICKS(30000) /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT 3                       /**< Number of attempts before giving up the connection parameter negotiation. */

#define SEC_PARAM_BOND 1                               /**< Perform bonding. */
#define SEC_PARAM_MITM 0                               /**< Man In The Middle protection not required. */
#define SEC_PARAM_LESC 0                               /**< LE Secure Connections not enabled. */
#define SEC_PARAM_KEYPRESS 0                           /**< Keypress notifications not enabled. */
#define SEC_PARAM_IO_CAPABILITIES BLE_GAP_IO_CAPS_NONE /**< No I/O capabilities. */
#define SEC_PARAM_OOB 0                                /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE 7                       /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE 16                      /**< Maximum encryption key size. */

#define DEAD_BEEF 0xDEADBEEF /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

BLE_BAS_DEF(m_bas);                      /**< Battery service instance. */
BLE_CSCS_DEF(m_cscs);                    /**< Cycling speed and cadence service instance. */
NRF_BLE_GATT_DEF(m_gatt);                /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                  /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);      /**< Advertising module instance. */
APP_TIMER_DEF(m_accelerometer_timer_id); /**< Battery timer. */
APP_TIMER_DEF(m_csc_meas_timer_id);      /**< CSC measurement timer. */

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID; /**< Handle of the current connection. */

static uint32_t m_cumulative_wheel_revs;    /**< Cumulative wheel revolutions. */
static bool m_auto_calibration_in_progress; /**< Set when an autocalibration is in progress. */

static ble_sensor_location_t supported_locations[] = /**< Supported location for the sensor location. */
    {
        BLE_SENSOR_LOCATION_FRONT_WHEEL,
        BLE_SENSOR_LOCATION_LEFT_CRANK,
        BLE_SENSOR_LOCATION_RIGHT_CRANK,
        BLE_SENSOR_LOCATION_LEFT_PEDAL,
        BLE_SENSOR_LOCATION_RIGHT_PEDAL,
        BLE_SENSOR_LOCATION_FRONT_HUB,
        BLE_SENSOR_LOCATION_REAR_DROPOUT,
        BLE_SENSOR_LOCATION_CHAINSTAY,
        BLE_SENSOR_LOCATION_REAR_WHEEL,
        BLE_SENSOR_LOCATION_REAR_HUB};

static ble_uuid_t m_adv_uuids[] = /**< Universally unique service identifiers. */
    {
        {BLE_UUID_CYCLING_SPEED_AND_CADENCE, BLE_UUID_TYPE_BLE},
        {BLE_UUID_BATTERY_SERVICE, BLE_UUID_TYPE_BLE},
        {BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE}};

/// some declarations
static void advertising_start(bool erase_bonds);
static void advertising_stop();

void read_sensor_data();
void gyro_turnOn();
void gyro_turnOff();
///

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num   Line number of the failing ASSERT call.
 * @param[in] file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

/**@brief Function for handling Peer Manager events.
 *
 * @param[in] p_evt  Peer Manager event.
 */
static void pm_evt_handler(pm_evt_t const *p_evt)
{
    pm_handler_on_pm_evt(p_evt);
    pm_handler_flash_clean(p_evt);

    switch (p_evt->evt_id)
    {
    case PM_EVT_PEERS_DELETE_SUCCEEDED:
        advertising_start(false);
        break;

    default:
        break;
    }
}

/**@brief Function for populating simulated cycling speed and cadence measurements.
 */
static void csc_measurement(ble_cscs_meas_t *p_measurement)
{
    static uint16_t event_time = 0;
    uint16_t event_time_inc;

    // Per specification event time is in 1/1024th's of a second.
    event_time_inc = (1024 * SPEED_AND_CADENCE_MEAS_INTERVAL) / 1000;

    // Calculate simulated wheel revolution values.
    p_measurement->is_wheel_rev_data_present = false;

    // mm_per_sec = KPH_TO_MM_PER_SEC * sensorsim_measure(&m_speed_kph_sim_state,
    //                                                    &m_speed_kph_sim_cfg);

    // wheel_revolution_mm     += mm_per_sec * SPEED_AND_CADENCE_MEAS_INTERVAL / 1000;
    // m_cumulative_wheel_revs += wheel_revolution_mm / WHEEL_CIRCUMFERENCE_MM;
    // wheel_revolution_mm     %= WHEEL_CIRCUMFERENCE_MM;

    // p_measurement->cumulative_wheel_revs = m_cumulative_wheel_revs;
    // p_measurement->last_wheel_event_time =
    //     event_time + (event_time_inc * (mm_per_sec - wheel_revolution_mm) / mm_per_sec);

    // Calculate simulated cadence values.
    p_measurement->is_crank_rev_data_present = true;

    rotationRead = true;
    read_sensor_data();

    while (!rotationRead)
    {
        nrf_delay_us(10);
    }

    event_time += event_time_inc;

    if (!CrankRevsChange)
    {
        NoChangeForMS += event_time_inc;
    }
    else
    {
        NoChangeForMS = 0;
    }

    p_measurement->cumulative_crank_revs = CrankRevs;
    p_measurement->last_crank_event_time =
        event_time; //+ (event_time_inc * (degrees_per_sec - crank_rev_degrees) / degrees_per_sec);
}

/**@brief Function for handling the Cycling Speed and Cadence measurement timer timeouts.
 *
 * @details This function will be called each time the cycling speed and cadence
 *          measurement timer expires.
 *
 * @param[in] p_context  Pointer used for passing some arbitrary information (context) from the
 *                       app_start_timer() call to the timeout handler.
 */
static void csc_meas_timeout_handler(void *p_context)
{
    if (IsIdle)
    {
        NRF_LOG_INFO("gyro timer unnecessary run.");
        return;
    }

    uint32_t err_code;
    ble_cscs_meas_t cscs_measurement;

    UNUSED_PARAMETER(p_context);

    csc_measurement(&cscs_measurement);

    if (CrankRevsChange)
    {
        CrankRevsChange = false;
        err_code = ble_cscs_measurement_send(&m_cscs, &cscs_measurement);
        if ((err_code != NRF_SUCCESS) &&
            (err_code != NRF_ERROR_INVALID_STATE) &&
            (err_code != NRF_ERROR_RESOURCES) &&
            (err_code != NRF_ERROR_BUSY) &&
            (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING))
        {
            APP_ERROR_HANDLER(err_code);
        }
        if (m_auto_calibration_in_progress)
        {
            err_code = ble_sc_ctrlpt_rsp_send(&(m_cscs.ctrl_pt), BLE_SCPT_SUCCESS);
            if ((err_code != NRF_SUCCESS) &&
                (err_code != NRF_ERROR_INVALID_STATE) &&
                (err_code != NRF_ERROR_RESOURCES))
            {
                APP_ERROR_HANDLER(err_code);
            }
            if (err_code != NRF_ERROR_RESOURCES)
            {
                m_auto_calibration_in_progress = false;
            }
        }
    }
}

/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    ret_code_t err_code;
    ble_gap_conn_params_t gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_CYCLING_SPEED_CADENCE_SENSOR);
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the GATT module.
 */
static void gatt_init(void)
{
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for handling Speed and Cadence Control point events
 *
 * @details Function for handling Speed and Cadence Control point events.
 *          This function parses the event and in case the "set cumulative value" event is received,
 *          sets the wheel cumulative value to the received value.
 */
ble_scpt_response_t sc_ctrlpt_event_handler(ble_sc_ctrlpt_t *p_sc_ctrlpt,
                                            ble_sc_ctrlpt_evt_t *p_evt)
{
    switch (p_evt->evt_type)
    {
    case BLE_SC_CTRLPT_EVT_SET_CUMUL_VALUE:
        m_cumulative_wheel_revs = p_evt->params.cumulative_value;
        break;

    case BLE_SC_CTRLPT_EVT_START_CALIBRATION:
        m_auto_calibration_in_progress = true;
        break;

    default:
        // No implementation needed.
        break;
    }
    return (BLE_SCPT_SUCCESS);
}

/**@brief Function for initializing services that will be used by the application.
 *
 * @details Initialize the Cycling Speed and Cadence, Battery and Device Information services.
 */
static void services_init(void)
{
    uint32_t err_code;
    ble_cscs_init_t cscs_init;
    ble_bas_init_t bas_init;
    ble_dis_init_t dis_init;
    ble_sensor_location_t sensor_location;
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    // Initialize Cycling Speed and Cadence Service.
    memset(&cscs_init, 0, sizeof(cscs_init));

    cscs_init.evt_handler = NULL;
    cscs_init.feature = BLE_CSCS_FEATURE_WHEEL_REV_BIT | BLE_CSCS_FEATURE_CRANK_REV_BIT |
                        BLE_CSCS_FEATURE_MULTIPLE_SENSORS_BIT;

    // Here the sec level for the Cycling Speed and Cadence Service can be changed/increased.
    cscs_init.csc_meas_cccd_wr_sec = SEC_OPEN;
    cscs_init.csc_feature_rd_sec = SEC_OPEN;
    cscs_init.csc_location_rd_sec = SEC_OPEN;
    cscs_init.sc_ctrlpt_cccd_wr_sec = SEC_OPEN;
    cscs_init.sc_ctrlpt_wr_sec = SEC_OPEN;

    cscs_init.ctrplt_supported_functions = BLE_SRV_SC_CTRLPT_CUM_VAL_OP_SUPPORTED | BLE_SRV_SC_CTRLPT_SENSOR_LOCATIONS_OP_SUPPORTED | BLE_SRV_SC_CTRLPT_START_CALIB_OP_SUPPORTED;
    cscs_init.ctrlpt_evt_handler = sc_ctrlpt_event_handler;
    cscs_init.list_supported_locations = supported_locations;
    cscs_init.size_list_supported_locations = sizeof(supported_locations) /
                                              sizeof(ble_sensor_location_t);

    sensor_location = BLE_SENSOR_LOCATION_RIGHT_CRANK; // initializes the sensor location to add the sensor location characteristic.
    cscs_init.sensor_location = &sensor_location;

    err_code = ble_cscs_init(&m_cscs, &cscs_init);
    APP_ERROR_CHECK(err_code);

    // Initialize Battery Service.
    memset(&bas_init, 0, sizeof(bas_init));

    // Here the sec level for the Battery Service can be changed/increased.
    bas_init.bl_rd_sec = SEC_OPEN;
    bas_init.bl_cccd_wr_sec = SEC_OPEN;
    bas_init.bl_report_rd_sec = SEC_OPEN;

    bas_init.evt_handler = NULL;
    bas_init.support_notification = true;
    bas_init.p_report_ref = NULL;
    bas_init.initial_batt_level = 100;

    err_code = ble_bas_init(&m_bas, &bas_init);
    APP_ERROR_CHECK(err_code);

    // Initialize Device Information Service.
    memset(&dis_init, 0, sizeof(dis_init));

    ble_srv_ascii_to_utf8(&dis_init.manufact_name_str, MANUFACTURER_NAME);

    dis_init.dis_char_rd_sec = SEC_OPEN;

    err_code = ble_dis_init(&dis_init);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for starting application timers.
 */
static void application_timers_start(void)
{
    ret_code_t err_code;

    // Start application timers.
    err_code = app_timer_start(m_accelerometer_timer_id, ACCELEROMETER_CHECK_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
}

static void gyroTimerStart()
{

    if (IsGyroTimerRunning)
    {
        return;
    }

    NRF_LOG_INFO("gyroTimerStart()");

    ret_code_t err_code;
    uint32_t csc_meas_timer_ticks;
    csc_meas_timer_ticks = APP_TIMER_TICKS(SPEED_AND_CADENCE_MEAS_INTERVAL);
    err_code = app_timer_start(m_csc_meas_timer_id, csc_meas_timer_ticks, NULL);
    APP_ERROR_CHECK(err_code);

    IsGyroTimerRunning = true;
}

static void gyroTimerStop()
{
    if (!IsGyroTimerRunning)
    {
        return;
    }

    // Stop timer if running
    ret_code_t err_code = app_timer_stop(m_csc_meas_timer_id);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_INFO("gyroTimerStop(): %d", err_code);

    IsGyroTimerRunning = false;
}

static void accelerometer_timeout_handler(void *p_context)
{
    UNUSED_PARAMETER(p_context);

    if (IsIdle == false && NoChangeForMS > TIMEOUT_TO_SLEEP)
    {
        NRF_LOG_INFO("GOING TO SLEEP. IsIdle = true");

        // 1. TURN OFF GYRO, TURN ON ACCELEROMETER
        gyroTimerStop();
        gyro_turnOff();

        // 2. DISCONNECT BLUETOOTH
        if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
        {
            //
            NRF_LOG_INFO("disconnecting bluetooth connection");
            ret_code_t err_code = sd_ble_gap_disconnect(m_conn_handle,
                                                        BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            NRF_LOG_INFO("disconnecting bluetooth connection err_code: %d", err_code);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
        }

        // 3. DISABLE BLUETOOTH ADVERTISING - if something was connected this wont work, but it will be disabled in the disconnected eventKD
        advertising_stop();

        // 4. GO TO SLEEP - done automagically
        IsIdle = true;
    }

    if (IsIdle == false && !IsGyroTimerRunning && !IsGyroInitialized)
    {
        if (!isBleAdvertisingOn)
        {
            NRF_LOG_INFO("starting ble advertising");
            // 1. start ble advertising
            advertising_start(false);
        }

        NRF_LOG_INFO("turning on gyro");
        // 2. turn on gyro
        gyro_turnOn();

        NRF_LOG_INFO("turning on gyro timer");
        // 3. start gyro measurements timer
        gyroTimerStart();
    }

    if (IsIdle)
    {
        NRF_LOG_INFO("accelerometer timer reading sensor data.");
        //read accelerometer
        read_sensor_data();
    }
}

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{
    ret_code_t err_code;

    // Initialize timer module.
    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    // // Create timers.
    err_code = app_timer_create(&m_accelerometer_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                accelerometer_timeout_handler);
    APP_ERROR_CHECK(err_code);

    // Create gyro timer.
    err_code = app_timer_create(&m_csc_meas_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                csc_meas_timeout_handler);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling the Connection Parameter events.
 *
 * @details This function will be called for all events in the Connection Parameters Module which
 *          are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by simply
 *                setting the disconnect_on_fail configuration parameter, but instead we use the
 *                event handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t *p_evt)
{
    ret_code_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}

/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t err_code;
    ble_conn_params_init_t connection_params_init;

    memset(&connection_params_init, 0, sizeof(connection_params_init));

    connection_params_init.p_conn_params = NULL;
    connection_params_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    connection_params_init.next_conn_params_update_delay = NEXT_CONN_PARAMS_UPDATE_DELAY;
    connection_params_init.max_conn_params_update_count = MAX_CONN_PARAMS_UPDATE_COUNT;
    connection_params_init.start_on_notify_cccd_handle = m_cscs.meas_handles.cccd_handle;
    connection_params_init.disconnect_on_fail = false;
    connection_params_init.evt_handler = on_conn_params_evt;
    connection_params_init.error_handler = conn_params_error_handler;

    err_code = ble_conn_params_init(&connection_params_init);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    ret_code_t err_code;

    switch (ble_adv_evt)
    {
    case BLE_ADV_EVT_FAST:
        NRF_LOG_INFO("Fast advertising");
        err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
        APP_ERROR_CHECK(err_code);
        break;

    case BLE_ADV_EVT_IDLE:
        NRF_LOG_INFO("ble_adv_idle");
        err_code = bsp_indication_set(BSP_INDICATE_IDLE);
        APP_ERROR_CHECK(err_code);
        isBleAdvertisingOn = false;
        break;
    default:
        break;
    }
}

/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context)
{
    ret_code_t err_code = NRF_SUCCESS;

    switch (p_ble_evt->header.evt_id)
    {
    case BLE_GAP_EVT_CONNECTED:

        if (IsIdle)
        {
            NRF_LOG_INFO("Client connected, but we are idle...");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            return;
        }

        NRF_LOG_INFO("Connected");
        err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
        APP_ERROR_CHECK(err_code);
        m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
        err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
        APP_ERROR_CHECK(err_code);
        break;

    case BLE_GAP_EVT_DISCONNECTED:
        NRF_LOG_INFO("Disconnected");
        m_conn_handle = BLE_CONN_HANDLE_INVALID;

        if(IsIdle) {
            //stop advertising
            advertising_stop();
        }

        break;

    case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
    {
        NRF_LOG_DEBUG("PHY update request.");
        ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
        err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
        APP_ERROR_CHECK(err_code);
    }
    break;

    case BLE_GATTC_EVT_TIMEOUT:
        // Disconnect on GATT Client timeout event.
        NRF_LOG_DEBUG("GATT Client Timeout.");
        err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                         BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        APP_ERROR_CHECK(err_code);
        break;

    case BLE_GATTS_EVT_TIMEOUT:
        // Disconnect on GATT Server timeout event.
        NRF_LOG_DEBUG("GATT Server Timeout.");
        err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                         BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        APP_ERROR_CHECK(err_code);
        break;

    default:
        // No implementation needed.
        break;
    }
}

/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}

/**@brief Function for the Peer Manager initialization.
 */
static void peer_manager_init(void)
{
    ble_gap_sec_params_t sec_param;
    ret_code_t err_code;

    err_code = pm_init();
    APP_ERROR_CHECK(err_code);

    memset(&sec_param, 0, sizeof(ble_gap_sec_params_t));

    // Security parameters to be used for all security procedures.
    sec_param.bond = SEC_PARAM_BOND;
    sec_param.mitm = SEC_PARAM_MITM;
    sec_param.lesc = SEC_PARAM_LESC;
    sec_param.keypress = SEC_PARAM_KEYPRESS;
    sec_param.io_caps = SEC_PARAM_IO_CAPABILITIES;
    sec_param.oob = SEC_PARAM_OOB;
    sec_param.min_key_size = SEC_PARAM_MIN_KEY_SIZE;
    sec_param.max_key_size = SEC_PARAM_MAX_KEY_SIZE;
    sec_param.kdist_own.enc = 1;
    sec_param.kdist_own.id = 1;
    sec_param.kdist_peer.enc = 1;
    sec_param.kdist_peer.id = 1;

    err_code = pm_sec_params_set(&sec_param);
    APP_ERROR_CHECK(err_code);

    err_code = pm_register(pm_evt_handler);
    APP_ERROR_CHECK(err_code);
}

/**@brief Clear bond information from persistent storage.
 */
static void delete_bonds(void)
{
    ret_code_t err_code;

    NRF_LOG_INFO("Erase bonds!");

    err_code = pm_peers_delete();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    ret_code_t err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance = true;
    init.advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    init.advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.advdata.uuids_complete.p_uuids = m_adv_uuids;

    init.config.ble_adv_fast_enabled = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout = APP_ADV_DURATION;

    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void)
{
    if (NRF_LOG_PROCESS() == false)
    {
        nrf_pwr_mgmt_run();
    }
}

/**@brief Function for starting advertising.
 */
static void advertising_start(bool erase_bonds)
{
    ret_code_t err_code;

    if (erase_bonds == true)
    {
        delete_bonds();
        // Advertising is started by PM_EVT_PEERS_DELETE_SUCCEEDED event.
    }
    else
    {
        err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
        APP_ERROR_CHECK(err_code);
        isBleAdvertisingOn = true;
    }
}

static void advertising_stop()
{

    if (isBleAdvertisingOn && m_conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        NRF_LOG_INFO("advertising_stop.");
        ret_code_t err_code = sd_ble_gap_adv_stop((&m_advertising)->adv_handle);
        APP_ERROR_CHECK(err_code);
        isBleAdvertisingOn = false;
    } else {
        NRF_LOG_INFO("cannot stop ble advertising.");
    }
}

//////////

void twi_handler(nrf_drv_twi_evt_t const *p_event, void *p_context)
{
    //NRF_LOG_INFO("twi handler %d", p_event->type);
    switch (p_event->type)
    {
    case NRF_DRV_TWI_EVT_DONE:
        if (p_event->xfer_desc.type == NRF_DRV_TWI_XFER_RX)
        {
            // for(int i = 0 ; i <  sizeof(m_sample); i++) {
            //    NRF_LOG_INFO("read%d: %d",i, m_sample[i]);
            // }

            //int temperature = m_sample[6] << 8 | m_sample[7];

            // no need for temperature
            //temperature = temperature / 340;
            //temperature += 36.53;
            //NRF_LOG_INFO("temperature=%d",temperature);

            if (IsIdle)
            {
                // check acc. readings

                int16_t accX = abs(m_sample[0] << 8 | m_sample[1]);
                int16_t accY = abs(m_sample[2] << 8 | m_sample[3]);
                int16_t accZ = abs(m_sample[4] << 8 | m_sample[5]);

                //NRF_LOG_INFO("Ax=%d; Ay=%d; Az=%d", accX, accY, accZ);

                NRF_LOG_INFO("acxprev: %d", accXPrev);

                if (accXPrev != 0 && accYPrev != 0 && accZPrev != 0)
                {
                    if (
                        abs(accXPrev - accX) >= ACC_SENSITIVITY ||
                        abs(accYPrev - accY) >= ACC_SENSITIVITY ||
                        abs(accZPrev - accZ) >= ACC_SENSITIVITY)
                    {
                        NRF_LOG_INFO("acxprev: %d", accXPrev);
                        NRF_LOG_INFO("acYprev: %d", accYPrev);
                        NRF_LOG_INFO("acZprev: %d", accZPrev);

                        NRF_LOG_INFO("acx: %d", accX);
                        NRF_LOG_INFO("acY: %d", accY);
                        NRF_LOG_INFO("acZ: %d", accZ);

                        NRF_LOG_INFO("accelerometer - movement detected. IsIdle = false.")
                        IsIdle = false;
                    }
                }

                if (IsIdle == false)
                {
                    // so it wont trigger change next time around
                    accXPrev = 0;
                    accYPrev = 0;
                    accZPrev = 0;
                }
                else
                {
                    accXPrev = accX;
                    accYPrev = accY;
                    accZPrev = accZ;
                }
            }
            else
            {
                // check gyro readings

                //int16_t gyrX = m_sample[8] << 8 | m_sample[9];
                //int16_t gyrY = m_sample[10] << 8 | m_sample[11];
                int16_t gyrZ = m_sample[12] << 8 | m_sample[13];
                double gyroMove = abs(gyrZ);         

                // NRF_LOG_INFO("X: %d", gyrX);
                // NRF_LOG_INFO("Y: %d", gyrY);
                // NRF_LOG_INFO("Z: %d", gyrZ);

                // react only after some artificial threashold
                if (gyroMove > 500)
                {
                    // degrees/s / time moving at that speed
                    //32768 = 1000 degrees per second
                    currentRotation += (gyroMove / (double)32.768) / (double)(1000 / SPEED_AND_CADENCE_MEAS_INTERVAL);

                    //NRF_LOG_INFO("gyroMove: %d", gyroMove);
                    //NRF_LOG_INFO("currentRotationZ: %d", currentRotation);
                }

                if (currentRotation >= 360)
                {
                    currentRotation = 0;
                    CrankRevs++;
                    CrankRevsChange = true;
                    NRF_LOG_INFO("360 degrees");
                }
            }
        }
        m_xfer_done = true;
        break;
    default:
        break;
    }
    rotationRead = true;
}

void twi_init(void)
{
    ret_code_t err_code;

    const nrf_drv_twi_config_t twi_lm75b_config = {
        .scl = 25,
        .sda = 29,
        .frequency = NRF_DRV_TWI_FREQ_100K,
        .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
        .clear_bus_init = false};

    err_code = nrf_drv_twi_init(&m_twi, &twi_lm75b_config, twi_handler, NULL);
    APP_ERROR_CHECK(err_code);

    nrf_drv_twi_enable(&m_twi);
    NRF_LOG_INFO("TWI sensor init done.");
}

void read_sensor_data()
{
    /* Writing to pointer byte. */
    uint8_t reg[1];
    reg[0] = 0x3B; // register with data
    m_xfer_done = false;
    ret_code_t err_code = 0;
    err_code = nrf_drv_twi_tx(&m_twi, MPU6050_ADDR, reg, sizeof(reg), false);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_INFO("read_sensor_data: register write failed");
        return;
    }
    while (m_xfer_done == false)
    {
    }

    m_xfer_done = false;
    // read everything -> accelerometer, temperature and gyro values (14 bytes)
    err_code = nrf_drv_twi_rx(&m_twi, MPU6050_ADDR, m_sample, sizeof(m_sample));
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_INFO("read_sensor_data: register read failed");
        return;
    }
}
////////

void gyro_turnOff()
{
    // just to be sure, timers dont stop immeadiatelly
    nrf_delay_ms(100);

    // reset crank revolutions
    CrankRevs = 0;
    NoChangeForMS = 0;
    CrankRevsChange = false;

    // cycle read mode
    uint8_t reg[2];
    reg[0] = 107;        // 107 - power management 1
    reg[1] = 0b00101000; // reset, sleep, cycle, reserva, temp_disable, clk, clk, clk
    m_xfer_done = false;
    ret_code_t err_code = nrf_drv_twi_tx(&m_twi, MPU6050_ADDR, reg, sizeof(reg), false);
    NRF_LOG_INFO("TWI write: %d", err_code);
    APP_ERROR_CHECK(err_code);
    while (m_xfer_done == false)
    {
    }

    nrf_delay_ms(10);

    // gyro axis to standby, the 3 zeros are 1.25 hz frequency of accelerometer reading
    reg[0] = 108;        // 108 - power management 2
    reg[1] = 0b00000111; // wake_freq, wake_freq, xa, ya, za, xg, yg, zg
    m_xfer_done = false;
    err_code = nrf_drv_twi_tx(&m_twi, MPU6050_ADDR, reg, sizeof(reg), false);
    NRF_LOG_INFO("TWI write: %d", err_code);
    APP_ERROR_CHECK(err_code);
    while (m_xfer_done == false)
    {
    }

    nrf_delay_ms(10);

    IsGyroInitialized = false;
}

void gyro_turnOn()
{
    if (IsGyroInitialized)
    {
        return;
    }

    // reset crank revolutions
    CrankRevs = 0;
    NoChangeForMS = 0;
    CrankRevsChange = false;

    ret_code_t err_code = 0;

    nrf_delay_ms(10);
    NRF_LOG_INFO("GYRO init started.");
    NRF_LOG_FLUSH();

    nrf_delay_ms(10);
    // turn off cycle mode
    uint8_t reg[2];
    reg[0] = 107;        // 107 - power management 1
    reg[1] = 0b00001000; // reset, sleep, cycle, reserved, temp_disable, clk, clk, clk
    m_xfer_done = false;
    err_code = nrf_drv_twi_tx(&m_twi, MPU6050_ADDR, reg, sizeof(reg), false);
    NRF_LOG_INFO("TWI write: %d", err_code);
    APP_ERROR_CHECK(err_code);
    while (m_xfer_done == false)
    {
    }

    //
    nrf_delay_ms(10);

    // turn on Z gyro axis and put accelerometer to standby
    reg[0] = 108;        // 108 - power management 2
    reg[1] = 0b00111110; // wake_freq, wake_freq, xa, ya, za, xg, yg, zg
    m_xfer_done = false;
    err_code = nrf_drv_twi_tx(&m_twi, MPU6050_ADDR, reg, sizeof(reg), false);
    NRF_LOG_INFO("TWI write: %d", err_code);
    APP_ERROR_CHECK(err_code);
    while (m_xfer_done == false)
    {
    }

    //
    nrf_delay_ms(10);

    //GYRO TO 1000 degrees per second
    reg[0] = 0x1B; //1000/s
    reg[1] = 0b00010000;
    m_xfer_done = false;
    err_code = nrf_drv_twi_tx(&m_twi, MPU6050_ADDR, reg, sizeof(reg), false);
    NRF_LOG_INFO("TWI write: %d", err_code);
    APP_ERROR_CHECK(err_code);
    while (m_xfer_done == false)
    {
    }

    nrf_delay_ms(50);
    IsGyroInitialized = true;
}

/**@brief Function for application main entry.
 */
int main(void)
{
    // Initialize.
    log_init();

    NRF_LOG_INFO("main start.");

    timers_init();
    power_management_init();
    ble_stack_init();
    gap_params_init();
    gatt_init();
    advertising_init();
    services_init();
    conn_params_init();
    peer_manager_init();

    /////////////
    NRF_LOG_INFO("TWI sensor init started.");
    NRF_LOG_FLUSH();
    twi_init();

    //init sensor
    gyro_turnOff();

    // Start execution.
    NRF_LOG_INFO("Cycling Speed and Cadence example started.");

    // zapne akcelerometrovy timer
    application_timers_start();

    // ble advertising start
    //advertising_start(erase_bonds);

    // Enter main loop.
    for (;;)
    {
        idle_state_handle();
    }
}