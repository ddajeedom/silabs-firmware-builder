/***************************************************************************//**
 * @file app.c
 * @brief Callbacks implementation and application specific code.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include "app/framework/include/af.h"
#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif

#ifdef SL_CATALOG_ZIGBEE_NETWORK_TEST_PRESENT
#include "network_test_config.h"
#endif //SL_CATALOG_ZIGBEE_NETWORK_TEST_PRESENT

#if (LARGE_NETWORK_TESTING == 0)

#include "btl_interface.h"
#include "network-steering.h"
#include "stack/include/zigbee-device-stack.h"
#include "app/framework/plugin/basic/basic.h"

#if defined(SL_CATALOG_LED0_PRESENT)
#include "sl_led.h"
#include "sl_simple_led_instances.h"

#define led_turn_on(led) sl_led_turn_on(led)
#define led_turn_off(led) sl_led_turn_off(led)
#define led_toggle(led) sl_led_toggle(led)
#define COMMISSIONING_STATUS_LED (&sl_led_led0)
#else // !SL_CATALOG_LED0_PRESENT
#define led_turn_on(led)
#define led_turn_off(led)
#define led_toggle(led)
#endif // SL_CATALOG_LED0_PRESENT

#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT)
#include "sl_simple_button.h"
#include "sl_simple_button_instances.h"
#include "stack/include/source-route.h"

#define BUTTON_LONG_PRESS_TIME_MS 5000
#define PERMIT_JOIN_TIMEOUT_SEC 180

static bool button_long_press = false;
static sl_zigbee_af_event_t button_event;
static bool source_route_discovery_off = false;
#endif // SL_CATALOG_SIMPLE_BUTTON_PRESENT

#define NWK_STEERING_COOLDOWN_MS 10000

static sl_zigbee_af_event_t commissioning_event;

//---------------
// Event handlers

static void commissioning_event_handler(sl_zigbee_af_event_t *event)
{
  (void)event;

  led_turn_on(COMMISSIONING_STATUS_LED);

  if (sl_zigbee_af_network_state() != SL_ZIGBEE_JOINED_NETWORK) {
    sl_status_t status = sl_zigbee_af_network_steering_start();
    sl_zigbee_app_debug_println("Join network start: 0x%X", status);
  }
}

#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT)
static void button_event_handler(sl_zigbee_af_event_t *event)
{
  (void)event;

  sl_zigbee_network_status_t state = sl_zigbee_af_network_state();

  if (state == SL_ZIGBEE_JOINED_NETWORK) {
    if (button_long_press) {
      sl_zigbee_app_debug_println("Btn long press: nwk up: leave network");
      sl_zigbee_leave_network(SL_ZIGBEE_LEAVE_NWK_WITH_NO_OPTION);
    } else {
      if (source_route_discovery_off) {
        source_route_discovery_off = false;
        sl_zigbee_app_debug_println("Btn press: nwk up: enable src route disc");
        sl_zigbee_set_source_route_discovery_mode(SL_ZIGBEE_SOURCE_ROUTE_DISCOVERY_ON);
      } else {
        source_route_discovery_off = true;
        sl_zigbee_app_debug_println("Btn press: nwk up: disable src route disc");
        sl_zigbee_set_source_route_discovery_mode(SL_ZIGBEE_SOURCE_ROUTE_DISCOVERY_OFF);
      }
    }
  } else if (state == SL_ZIGBEE_NO_NETWORK) {
    if (button_long_press) {
      sl_zigbee_app_debug_println("Btn long press: nwk down: reboot btl");
      bootloader_rebootAndInstall();
    } else {
      if (sl_zigbee_af_event_is_scheduled(&commissioning_event)) {
        sl_zigbee_app_debug_println("Btn press: nwk down: stop nwk steering");
        sl_zigbee_af_event_set_inactive(&commissioning_event);
      } else {
        sl_zigbee_app_debug_println("Btn press: nwk down: start nwk steering");
        sl_zigbee_af_event_set_active(&commissioning_event);
      }
    }
  }
}
#endif // SL_CATALOG_SIMPLE_BUTTON_PRESENT

//----------------------
// Implemented Callbacks

/** @brief Stack Status
 *
 * This function is called by the application framework from the stack status
 * handler.  This callbacks provides applications an opportunity to be notified
 * of changes to the stack status and take appropriate action. The framework
 * will always process the stack status after the callback returns.
 */
void sl_zigbee_af_stack_status_cb(sl_status_t status)
{
  if (status == SL_STATUS_NETWORK_DOWN) {
    led_turn_off(COMMISSIONING_STATUS_LED);
    sl_zigbee_af_event_set_delay_ms(&commissioning_event, NWK_STEERING_COOLDOWN_MS);
  } else if (status == SL_STATUS_NETWORK_UP) {
    // make some noise on start for reconnect
    // (otherwise perceived as offline until pinged)
    sl_zigbee_send_device_announcement();
    led_turn_on(COMMISSIONING_STATUS_LED);
  }
}

/** @brief Init
 * Application init function
 */
static void sav_cover_register_cli(void);
static void sav_gp_register_cli(void);

void sl_zigbee_af_main_init_cb(void)
{
  sav_cover_register_cli();
  sav_gp_register_cli();
  sl_zigbee_af_event_init(&commissioning_event, commissioning_event_handler);

  #if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT)
  sl_zigbee_af_isr_event_init(&button_event, button_event_handler);
  #endif // SL_CATALOG_SIMPLE_BUTTON_PRESENT

  sl_zigbee_af_event_set_active(&commissioning_event);
}

/** @brief Complete network steering.
 *
 * This callback is fired when the Network Steering plugin is complete.
 *
 * @param status On success this will be set to SL_STATUS_OK to indicate a
 * network was joined successfully. On failure this will be the status code of
 * the last join or scan attempt.
 *
 * @param totalBeacons The total number of 802.15.4 beacons that were heard,
 * including beacons from different devices with the same PAN ID.
 *
 * @param joinAttempts The number of join attempts that were made to get onto
 * an open Zigbee network.
 *
 * @param finalState The finishing state of the network steering process. From
 * this, one is able to tell on which channel mask and with which key the
 * process was complete.
 */
void sl_zigbee_af_network_steering_complete_cb(sl_status_t status,
                                               uint8_t totalBeacons,
                                               uint8_t joinAttempts,
                                               uint8_t finalState)
{
  (void)totalBeacons;
  (void)joinAttempts;
  (void)finalState;

  sl_zigbee_app_debug_println("Join network complete: 0x%X", status);

  if (status != SL_STATUS_OK) {
    led_turn_off(COMMISSIONING_STATUS_LED);
    sl_zigbee_af_event_set_delay_ms(&commissioning_event, NWK_STEERING_COOLDOWN_MS);
  }
}

/** @brief Reset To Factory Defaults
 *
 * This function is called by the Basic server plugin when a request to reset
 * to factory defaults is received. The plugin will reset attributes managed by
 * the framework to their default values. The application should perform any
 * other necessary reset-related operations in this callback, including
 * resetting any externally-stored attributes.
 *
 * @param endpoint   Ver.: always
 */
void sl_zigbee_af_basic_reset_to_factory_defaults_cb(uint8_t endpoint)
{
  (void)endpoint;

  bootloader_rebootAndInstall();
}

/** @brief
 *
 * Application framework equivalent of ::sl_zigbee_radio_needs_calibrating_handler
 */
void sl_zigbee_af_radio_needs_calibrating_cb(void)
{
  #ifndef EZSP_HOST
  sl_mac_calibrate_current_channel();
  #endif
}

#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT)
/***************************************************************************//**
 * A callback called in interrupt context whenever a button changes its state.
 *
 * @remark Can be implemented by the application if required. This function
 * can contain the functionality to be executed in response to changes of state
 * in each of the buttons, or callbacks to appropriate functionality.
 *
 * @note The button state should not be updated in this function, it is updated
 * by specific button driver prior to arriving here
 *
   @param[out] handle             Pointer to button instance
 ******************************************************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  static uint16_t button_press_time;
  uint16_t current_time = 0;
  sl_button_state_t button_state = sl_button_get_state(handle);

  if (button_state == SL_SIMPLE_BUTTON_PRESSED) {
    button_press_time = halCommonGetInt16uMillisecondTick();
  } else if (button_state == SL_SIMPLE_BUTTON_RELEASED) {
    current_time = halCommonGetInt16uMillisecondTick();
    button_long_press = (current_time - button_press_time) > BUTTON_LONG_PRESS_TIME_MS;

    sl_zigbee_af_event_set_active(&button_event);
  }
}
#endif // SL_CATALOG_SIMPLE_BUTTON_PRESENT

#endif //#if (LARGE_NETWORK_TESTING == 0)


/* SAV / pilotage ZCL Window Covering, CLI volontairement explicite.
 * Prototype experimental : a compiler ET valider avant tout flash.
 * Ne modifie pas le role routeur, les cles ou le commissionnement. */
#include "sl_cli.h"
#include "sl_cli_instances.h"
#include <stdint.h>

static void sav_cover_cmd(sl_cli_command_arg_t *args)
{
  const uint16_t target = sl_cli_get_argument_uint16(args, 0);
  const uint8_t endpoint = sl_cli_get_argument_uint8(args, 1);
  const uint8_t action = sl_cli_get_argument_uint8(args, 2);
  const uint8_t source_endpoint = 1U; /* A confirmer dans zcl_config.zap. */

  if (sl_zigbee_af_network_state() != SL_ZIGBEE_JOINED_NETWORK) {
    sl_zigbee_app_debug_println("[SAV-COVER] REFUS: SONOFF hors reseau");
    return;
  }
  if (target == 0U || target >= 0xFFF8U || endpoint == 0U || endpoint == 255U || action > 2U) {
    sl_zigbee_app_debug_println("[SAV-COVER] REFUS: cible/endpoint/action invalides");
    return;
  }
  uint16_t length = 0U;
  switch (action) {
    case 0U: length = sl_zigbee_af_fill_command_window_covering_cluster_window_covering_up_open(); break;
    case 1U: length = sl_zigbee_af_fill_command_window_covering_cluster_window_covering_stop(); break;
    case 2U: length = sl_zigbee_af_fill_command_window_covering_cluster_window_covering_down_close(); break;
    default: return;
  }
  if (length == 0U) {
    sl_zigbee_app_debug_println("[SAV-COVER] REFUS: remplissage ZCL impossible");
    return;
  }
  sl_zigbee_af_set_command_endpoints(source_endpoint, endpoint);
  sl_status_t result = sl_zigbee_af_send_command_unicast(SL_ZIGBEE_OUTGOING_DIRECT, target);
  sl_zigbee_app_debug_println("[SAV-COVER] TX addr=0x%04X ep=%u action=%u status=0x%lX (reception moteur non confirmee)",
                              target, endpoint, action, (unsigned long)result);
}

static const struct {
  sl_cli_command_func_t function;
#if SL_CLI_HELP_DESCRIPTION_ENABLED
  char *help;
  char *arg_help;
#endif
  sl_cli_argument_type_t arg_type_list[4];
} sav_cover_info = {
  .function = sav_cover_cmd,
#if SL_CLI_HELP_DESCRIPTION_ENABLED
  .help = "Window Covering test: action 0=up, 1=stop, 2=down",
  .arg_help = "NWK(uint16) endpoint(uint8) action(uint8)",
#endif
  .arg_type_list = {SL_CLI_ARG_UINT16, SL_CLI_ARG_UINT8, SL_CLI_ARG_UINT8, SL_CLI_ARG_END}
};
static const sl_cli_command_entry_t sav_cover_table[] = {
  { "sav_cover", (const sl_cli_command_info_t *)&sav_cover_info, false },
  { NULL, NULL, false }
};
static sl_cli_command_group_t sav_cover_group = { { NULL }, false, sav_cover_table };
static void sav_cover_register_cli(void)
{
  if (!sl_cli_command_add_command_group(sl_cli_default_handle, &sav_cover_group)) {
    sl_zigbee_app_debug_println("[SAV-COVER] CLI enregistrement impossible");
  }
}


/* SAV Green Power: diagnostics only. No radio scan, pairing, key reads,
 * factory reset or network changes are performed by these commands. */
static bool sav_gp_watch_enabled = false;
static uint32_t sav_gp_pairing_event_count = 0U;
static uint32_t sav_gp_timeout_event_count = 0U;

/* Standard GP Server callbacks, SDK 9.x. Successful linkage must be verified
 * by the GitHub compiler; callbacks do not establish Profalux compatibility. */
void sl_zigbee_af_green_power_server_pairing_complete_cb(uint8_t numberOfEndpoints,
                                                          uint8_t *endpoints)
{
  ++sav_gp_pairing_event_count;
  if (!sav_gp_watch_enabled) { return; }
  sl_zigbee_app_debug_println("[SAV-GP] PAIRING_COMPLETE count=%lu endpoints=%u",
                              (unsigned long)sav_gp_pairing_event_count,
                              numberOfEndpoints);
  for (uint8_t i = 0U; (i < numberOfEndpoints) && (endpoints != NULL); ++i) {
    sl_zigbee_app_debug_println("[SAV-GP] paired-endpoint=%u", endpoints[i]);
  }
}

void sl_zigbee_af_green_power_server_commissioning_timeout_cb(uint8_t commissioningTimeoutType,
                                                               uint8_t numberOfEndpoints,
                                                               uint8_t *endpoints)
{
  (void)endpoints;
  ++sav_gp_timeout_event_count;
  if (!sav_gp_watch_enabled) { return; }
  sl_zigbee_app_debug_println("[SAV-GP] COMMISSIONING_TIMEOUT count=%lu type=%u endpoints=%u",
                              (unsigned long)sav_gp_timeout_event_count,
                              commissioningTimeoutType,
                              numberOfEndpoints);
}

static void sav_gp_cmd(sl_cli_command_arg_t *args)
{
  (void)args;
  sl_zigbee_app_debug_println("[SAV-GP] DIAGNOSTIC READ ONLY; not a discovery scan");
  sl_zigbee_app_debug_println("[SAV-GP] watch=%s", sav_gp_watch_enabled ? "ON" : "OFF");
  sl_zigbee_app_debug_println("[SAV-GP] network=%s endpoint_gp=242",
                              sl_zigbee_af_network_state() == SL_ZIGBEE_JOINED_NETWORK
                              ? "JOINED" : "NOT_JOINED");
  sl_zigbee_app_debug_println("[SAV-GP] pairing_events=%lu commissioning_timeouts=%lu",
                              (unsigned long)sav_gp_pairing_event_count,
                              (unsigned long)sav_gp_timeout_event_count);
  sl_zigbee_app_debug_println("[SAV-GP] No automatic commissioning or Profalux detection");
}

/* Toggle logging of the existing Green Power Server callbacks.
 * 1 = print callback events, 0 = silence; counters always accumulate.
 * Does not open a commissioning window or scan the radio. */
static void sav_gp_watch_cmd(sl_cli_command_arg_t *args)
{
  const uint8_t mode = sl_cli_get_argument_uint8(args, 0);
  if (mode > 1U) {
    sl_zigbee_app_debug_println("[SAV-GP] WATCH invalid value (use 0 or 1)");
    return;
  }
  sav_gp_watch_enabled = (mode == 1U);
  sl_zigbee_app_debug_println("[SAV-GP] WATCH %s (callback logs only; no discovery)",
                              sav_gp_watch_enabled ? "ON" : "OFF");
}

static const struct {
  sl_cli_command_func_t function;
#if SL_CLI_HELP_DESCRIPTION_ENABLED
  char *help;
  char *arg_help;
#endif
  sl_cli_argument_type_t arg_type_list[2];
} sav_gp_watch_info = {
  .function = sav_gp_watch_cmd,
#if SL_CLI_HELP_DESCRIPTION_ENABLED
  .help = "Toggle Green Power callback logging (1=on, 0=off)",
  .arg_help = "mode(uint8)",
#endif
  .arg_type_list = {SL_CLI_ARG_UINT8, SL_CLI_ARG_END}
};

static const struct {
  sl_cli_command_func_t function;
#if SL_CLI_HELP_DESCRIPTION_ENABLED
  char *help;
  char *arg_help;
#endif
  sl_cli_argument_type_t arg_type_list[1];
} sav_gp_info = {
  .function = sav_gp_cmd,
#if SL_CLI_HELP_DESCRIPTION_ENABLED
  .help = "Green Power SAV diagnostic (read only)",
  .arg_help = "",
#endif
  .arg_type_list = {SL_CLI_ARG_END}
};
static const sl_cli_command_entry_t sav_gp_table[] = {
  { "sav_gp", (const sl_cli_command_info_t *)&sav_gp_info, false },
  { "sav_gp_watch", (const sl_cli_command_info_t *)&sav_gp_watch_info, false },
  { NULL, NULL, false }
};
static sl_cli_command_group_t sav_gp_group = { { NULL }, false, sav_gp_table };
static void sav_gp_register_cli(void)
{
  if (!sl_cli_command_add_command_group(sl_cli_default_handle, &sav_gp_group)) {
    sl_zigbee_app_debug_println("[SAV-GP] CLI registration failed");
  }
}
