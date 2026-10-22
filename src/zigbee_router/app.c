
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
