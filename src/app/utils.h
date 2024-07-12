#pragma once

#define repeat(n) for(int i = n; i--;)

struct tcp_pcb;
extern struct tcp_pcb* tcp_tw_pcbs;
extern "C" void tcp_abort(struct tcp_pcb* pcb);

void tcpCleanup(void)
    {
        // Закрытие открытых неиспользуемых портов tcp.
        // Это делать не ранее чем через 2 секунды, после выключения web сервера.

        while(tcp_tw_pcbs) {
            tcp_abort(tcp_tw_pcbs);
		}
    }

void wait(uint32_t milliseconds)
    {
        vTaskDelay(milliseconds / portTICK_RATE_MS);
    }
