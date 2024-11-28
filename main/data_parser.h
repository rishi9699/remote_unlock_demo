void initiate_uart(void);
void parse_received_data(const uint8_t *const data, const int rxBytes);
void rx_task(void *arg);
void comm_initiator();