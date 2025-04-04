#include <stdio.h>
#include "esp-qweb.h"

#include "utility.h"

#define CONFIG_TEST_AP

static const char test_content[] = 
    "Hello World!";


void app_main(void)
{
    
#ifdef CONFIG_TEST_AP
    start_test_ap();
#elif defined(CONFIG_TEST_SSID) && defined(CONFIG_TEST_PWD)
    connect_for_test(CONFIG_TEST_SSID, CONFIG_TEST_PWD);
#else
#error "No connection method included"
#endif


#ifdef CONFIG_QWEB_EN_SSL
    qweb_server_config_t cfg = QWEB_SSL_SERVER_CFG_DEFAULT("qweb ssl test");
#else
    qweb_server_config_t cfg = QWEB_SERVER_CFG_DEFAULT("qweb test");
#endif

    qweb_server_t *server = qweb_init(&cfg);
    qweb_register_file(server, "/", HTTP_MIME_HTML, test_content, sizeof(test_content));

}