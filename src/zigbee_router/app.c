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
