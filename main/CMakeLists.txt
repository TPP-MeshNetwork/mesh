idf_component_register(SRCS "mesh_main.c"
                            "mesh_netif.c"
                            "nvs_helper.c"
                            "mqtt/mqtt_mutual_auth.c"
                     INCLUDE_DIRS "." 
                                 "include"
                                 "mqtt"
                        )
