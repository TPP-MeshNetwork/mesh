idf_component_register(SRCS # Main files Mesh
                            "mesh_main.c"
                            "mesh_netif/mesh_netif.c"
                            # Network manager files
                            "network_manager/provisioning.c"
                            "persistence/persistence.c"
                            # MQTT client & utils files
                            "mqtt/client/mqtt_mutual_auth.c"
                            "mqtt/utils/mqtt_utils.c"
                            "mqtt/mqtt_queue.c"
                            "suscription_handlers/suscription_event_handlers.c"
                            # Sensor files Libraries
                            "components/dht.c"
                            "sensors/utils/sensor_utils.c"
                            "sensors/tasks/task_sensor_dht11.c"
                            # Performance files
                            "performance/performance.c"
                            # Tasks config files
                            "tasks_config/tasks_config.c"
                            # Utils files
                            "utils/utils.c"
                     INCLUDE_DIRS "." 
                                 "mesh_netif"
                                 "mqtt"
                                 "network_manager"
                                 "persistence"
                                 "suscription_handlers"
                                 "performance"
                                 "sensors"
                                 "tasks_config"
                                 "utils"
                        )
