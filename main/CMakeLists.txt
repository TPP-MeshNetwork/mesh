idf_component_register(SRCS "mesh_main.c"
                            "mesh_netif.c"
                            "mqtt_app.c"
                            "nvs_helper.c"
                            "mqtt_demo_mutual_auth.c"
                            "mqtt/aws_initialize_utils.c"
                            "mqtt/aws_subscribe_utils.c"
                            "mqtt/aws_publish_utils.c"
                    INCLUDE_DIRS "." "include")
