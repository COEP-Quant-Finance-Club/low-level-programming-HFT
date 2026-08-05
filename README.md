Welcome to the repo where things get more interesting, this repo is a gem for aspiring quant developers
There are primarily 2 parts to HFT
1] software optimization (eg, lock free data structures, linux optimization, execution algos, etc)
2] Hardware optimization (Eg Kernel bypass, FPGA programming, Physical server etc)
This repo primarily deals with point 1], however in future any pet projects related to hardware being made, their documentation can be added here
PLEASE NOTE: It is impossible for a student led club to achieve true HFT latencies, as HFT firms have direct market access, broker liscences, clearing and settlement 
support, and most importantly "COLOCATION", which allows these firms to place their servers directly within the exchange.
True HFT firms (Unlike some firms that "Larp" that they do HFT) have "End to End" latencies of less than 100 nanoseconds, and we dont know they may be going 
lower than that as JaneStreet never tells all of it publicly, but they optimize to the lowest level of CPU clock cycles
But here what we can realistically achieve either using hardware and software:
Level 1 — Typical College Algo Club (What most clubs build)

Usually:

Python
Broker API
Technical indicators
Moving averages
Streamlit dashboard

Latency:

100–500 ms

Portfolio value:

⭐☆☆☆☆

This won't impress serious quant firms much.

Level 2 — What I think COEP Quant Club could realistically build

This is what I'd aim for.

Software
-C++20
-Event-driven architecture
-OMS
-EMS
-Replay engine
-Risk engine
-Strategy plug-in framework
-Lock-free queues (where justified)
-Tick-by-tick processing

Deployment:

Linux VPS in Mumbai or Location nearest to the exchange (depends on what markets you trade) 

Strategies:

Quantitative scalping
Mean reversion
Momentum
Order flow prediction

Latency:

Component	Target
Internal	50–100 µs
End-to-end	2–10 ms (broker dependent)

This would genuinely be an impressive engineering project.

Level 3 — "Maximum Retail"

Suppose the club spends money.

Hardware:

Dedicated Linux server
Intel i9 / Ryzen 9
Good NIC
Linux tuning
CPU pinning
Huge pages
Better broker
VPS or dedicated server near Mumbai

Now

Internal latency

20–50 µs

End-to-end

1–5 ms

I think this is close to the ceiling for a retail-accessible setup using broker APIs.

Can we beat professional HFT?

No.

Because the bottleneck becomes

Broker

↓

Exchange

which we don't control.

So yeah, the real skill is in reducing the systems internal latency, linux optimization for VPS deployment, choice of VPS, Etc, unless we decide to get DMA or Co-Loc which is now becoming more and more lucrative to small teams, so keep your eyes open on latest services provided!.
