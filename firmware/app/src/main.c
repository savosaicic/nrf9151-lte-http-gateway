#include <dk_buttons_and_leds.h>
#include <ncs_version.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>

#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>

LOG_MODULE_REGISTER(nrf9151Gateway, LOG_LEVEL_INF);

#define SERVER_HOSTNAME   "YOURDOMAIN"
#define SERVER_PORT       5000
#define DEVICE_ID         "01"
#define HTTP_TIMEOUT_MS   (10 * MSEC_PER_SEC)
#define RECV_BUF_LEN      512
#define PAYLOAD_BUF_LEN   256
#define MAIN_LOOP_SLEEP_MS 200


static ATOMIC_DEFINE(button_pending, 1);
static uint8_t recv_buf[RECV_BUF_LEN];

static K_SEM_DEFINE(lte_connected, 0, 1);


static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	if (button_state & DK_BTN1_MSK) {
		LOG_INF("Button 1 pressed");
		dk_set_led_on(DK_LED1);
		atomic_set_bit(button_pending, 0);
	} else {
		LOG_INF("Button 1 released");
		dk_set_led_off(DK_LED1);
	}
}

static void lte_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_NW_REG_STATUS:
		if (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME &&
		    evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING) {
			break;
		}
		LOG_INF("Network registration status: %s",
			evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME
				? "Connected - home network"
				: "Connected - roaming");
		k_sem_give(&lte_connected);
		break;

	case LTE_LC_EVT_RRC_UPDATE:
		LOG_INF("RRC mode: %s",
			evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED
				? "Connected"
				: "Idle");
		break;

	default:
		break;
	}
}

static int modem_configure(void)
{
  int err;

  LOG_INF("Initializing modem library");
  err = nrf_modem_lib_init();
  if (err) {
    LOG_ERR("Failed to initialize modem library");
    return (err);
  }

  LOG_INF("Connecting to LTE network");
  err = lte_lc_connect_async(lte_handler);
  if (err) {
    LOG_ERR("Error in lte_lc_connect_async");
    return (err);
  }
  return (0);
}

static int http_response_cb(struct http_response *rsp,
                            enum http_final_call final_data, void *user_data) {
  if (final_data == HTTP_DATA_MORE) {
    LOG_INF("Partial data received (%zd bytes)", rsp->data_len);
  } else if (final_data == HTTP_DATA_FINAL) {
    LOG_INF("All the data received (%zd bytes)", rsp->data_len);
  }

  LOG_INF("Response status: %d %s", rsp->http_status_code, rsp->http_status);

  if (rsp->http_status_code == 201) {
    LOG_INF("Request successful!");
  } else {
    LOG_WRN("Request returned non-201 status");
  }

  return (0);
}

static int setup_socket(int *sock, struct sockaddr *addr, socklen_t *addr_len)
{
	char port_str[6];
	struct addrinfo hints = {
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};
	struct addrinfo *res;
	int ret;

	snprintf(port_str, sizeof(port_str), "%d", SERVER_PORT);

	ret = getaddrinfo(SERVER_HOSTNAME, port_str, &hints, &res);
	if (ret != 0) {
		LOG_ERR("getaddrinfo failed: %s", gai_strerror(ret));
		return (-ENOENT);
	}

	*sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (*sock < 0) {
		LOG_ERR("Failed to create socket: %d", errno);
		freeaddrinfo(res);
		return (-errno);
	}

	memcpy(addr, res->ai_addr, res->ai_addrlen);
	*addr_len = res->ai_addrlen;

	freeaddrinfo(res);
	return (0);
}

static int send_http_post(const char *payload)
{
	static const char *headers[] = {
		"Content-Type: application/json\r\n",
		NULL,
	};

	struct sockaddr_storage addr;
	socklen_t addr_len;
	int sock = -1;
	int ret;

	ret = setup_socket(&sock, (struct sockaddr *)&addr, &addr_len);
	if (ret < 0) {
		LOG_ERR("Failed to setup socket: %d", ret);
		return (ret);
	}

	ret = connect(sock, (struct sockaddr *)&addr, addr_len);
	if (ret < 0) {
		LOG_ERR("Cannot connect to server: %d", errno);
		close(sock);
		return (-errno);
	}

	LOG_INF("Connected to %s:%d", SERVER_HOSTNAME, SERVER_PORT);

	struct http_request req = {
		.method       = HTTP_POST,
		.url          = "/api/events",
		.host         = SERVER_HOSTNAME,
		.protocol     = "HTTP/1.1",
		.header_fields = headers,
		.payload      = payload,
		.payload_len  = strlen(payload),
		.response     = http_response_cb,
		.recv_buf     = recv_buf,
		.recv_buf_len = sizeof(recv_buf),
	};

	LOG_INF("Sending HTTP POST: %s", payload);

	ret = http_client_req(sock, &req, HTTP_TIMEOUT_MS, NULL);
	if (ret < 0) {
		LOG_ERR("HTTP request failed: %d", ret);
	}

	close(sock);
	return (ret);
}

static void build_button_payload(char *buf, size_t buf_size, int button_id) {
  snprintf(buf, buf_size,
           "{\"event_type\":\"button_press\",\"data\":{\"button\":%d,\"device_"
           "id\":\"nrf9151_%s\"}}",
           button_id, DEVICE_ID);
}

int main(void)
{
	int err;
	char payload[PAYLOAD_BUF_LEN];

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Failed to initialize LEDs: %d", err);
		return (err);
	}

	err = dk_buttons_init(button_changed);
	if (err) {
		LOG_ERR("Failed to initialize buttons: %d", err);
		return (err);
	}

	err = modem_configure();
	if (err) {
		LOG_ERR("Failed to configure modem: %d", err);
		return (err);
	}

	k_sem_take(&lte_connected, K_FOREVER);
	LOG_INF("Connected to LTE network");
	dk_set_led_on(DK_LED2);

	while (1) {
		if (atomic_test_and_clear_bit(button_pending, 0)) {
			build_button_payload(payload, sizeof(payload), 1);

			err = send_http_post(payload);
			if (err < 0) {
				LOG_ERR("Failed to send HTTP request: %d", err);
			}
		}

		k_msleep(MAIN_LOOP_SLEEP_MS);
	}

	return (0);
}
