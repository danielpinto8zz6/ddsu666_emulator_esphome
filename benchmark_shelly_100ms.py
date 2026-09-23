import asyncio
import json
import time
import csv
import sys
import statistics
import websockets

SHELLY_WS_URL = "ws://10.0.0.187/rpc"
TEST_DURATION_SEC = 30
INTERVAL_SEC = 0.100  # 100ms
CSV_FILENAME = "shelly_100ms_benchmark.csv"

async def run_benchmark():
    print(f"Connecting to {SHELLY_WS_URL} ...")
    try:
        ws = await websockets.connect(SHELLY_WS_URL, close_timeout=2)
    except Exception as e:
        print(f"Failed to connect to Shelly: {e}")
        return

    print(f"Connected! Starting 30-second benchmark (querying every 100ms)...")
    records = []
    start_time = time.time()
    end_time = start_time + TEST_DURATION_SEC
    msg_id = 0

    next_tick = time.time()

    while time.time() < end_time:
        msg_id += 1
        req = {
            "id": msg_id,
            "method": "EM1.GetStatus",
            "params": {"id": 0}
        }
        
        t0 = time.perf_counter()
        try:
            await ws.send(json.dumps(req))
            resp_raw = await asyncio.wait_for(ws.recv(), timeout=0.5)
            rtt_ms = (time.perf_counter() - t0) * 1000.0
            
            resp = json.loads(resp_raw)
            res = resp.get("result", {})
            voltage = res.get("voltage", 0.0)
            current = res.get("current", 0.0)
            act_power = res.get("act_power", 0.0)
            aprt_power = res.get("aprt_power", 0.0)
            pf = res.get("pf", 0.0)
            freq = res.get("freq", 0.0)
            success = True
            error_str = ""
        except asyncio.TimeoutError:
            rtt_ms = (time.perf_counter() - t0) * 1000.0
            voltage = current = act_power = aprt_power = pf = freq = None
            success = False
            error_str = "TIMEOUT"
        except Exception as e:
            rtt_ms = (time.perf_counter() - t0) * 1000.0
            voltage = current = act_power = aprt_power = pf = freq = None
            success = False
            error_str = str(e)

        records.append({
            "timestamp": time.time(),
            "elapsed_sec": round(time.time() - start_time, 3),
            "msg_id": msg_id,
            "rtt_ms": round(rtt_ms, 2),
            "success": success,
            "voltage": voltage,
            "current": current,
            "act_power": act_power,
            "aprt_power": aprt_power,
            "pf": pf,
            "freq": freq,
            "error": error_str
        })

        if msg_id % 50 == 0:
            sys.stdout.write(f"\rProgress: {round(time.time() - start_time, 1)}s / {TEST_DURATION_SEC}s ({msg_id} queries, latest power: {act_power}W, RTT: {rtt_ms:.1f}ms)...")
            sys.stdout.flush()

        next_tick += INTERVAL_SEC
        sleep_dur = next_tick - time.time()
        if sleep_dur > 0:
            await asyncio.sleep(sleep_dur)
        else:
            next_tick = time.time()

    await ws.close()
    print(f"\nBenchmark complete! Writing to {CSV_FILENAME} ...")

    with open(CSV_FILENAME, mode="w", newline="") as f:
        fieldnames = ["timestamp", "elapsed_sec", "msg_id", "rtt_ms", "success", "voltage", "current", "act_power", "aprt_power", "pf", "freq", "error"]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for r in records:
            writer.writerow(r)

    # Analysis
    successful_rtts = [r["rtt_ms"] for r in records if r["success"]]
    failures = len(records) - len(successful_rtts)
    
    print("\n" + "="*55)
    print("           SHELLY PRO EM 100MS BENCHMARK REPORT")
    print("="*55)
    print(f"Total Duration:        {records[-1]['elapsed_sec']:.2f} seconds")
    print(f"Total Queries Sent:    {len(records)}")
    print(f"Successful Queries:    {len(successful_rtts)} ({len(successful_rtts)/len(records)*100:.1f}%)")
    print(f"Failed / Timed Out:    {failures}")

    if successful_rtts:
        successful_rtts.sort()
        mean_rtt = statistics.mean(successful_rtts)
        median_rtt = statistics.median(successful_rtts)
        stdev_rtt = statistics.stdev(successful_rtts) if len(successful_rtts) > 1 else 0
        p90 = successful_rtts[int(len(successful_rtts) * 0.90)]
        p95 = successful_rtts[int(len(successful_rtts) * 0.95)]
        p99 = successful_rtts[int(len(successful_rtts) * 0.99)]
        min_rtt = successful_rtts[0]
        max_rtt = successful_rtts[-1]

        print(f"\n--- Latency / Round-Trip Time (RTT) ---")
        print(f"Min RTT:               {min_rtt:.2f} ms")
        print(f"Median RTT:            {median_rtt:.2f} ms")
        print(f"Mean RTT:              {mean_rtt:.2f} ms")
        print(f"90th Percentile (P90): {p90:.2f} ms")
        print(f"95th Percentile (P95): {p95:.2f} ms")
        print(f"99th Percentile (P99): {p99:.2f} ms")
        print(f"Max RTT:               {max_rtt:.2f} ms")
        print(f"Jitter (StdDev):       {stdev_rtt:.2f} ms")

    # Value update analysis (freshness)
    power_values = [r["act_power"] for r in records if r["success"] and r["act_power"] is not None]
    if len(power_values) > 1:
        changes = sum(1 for i in range(1, len(power_values)) if power_values[i] != power_values[i-1])
        diffs = [abs(power_values[i] - power_values[i-1]) for i in range(1, len(power_values)) if power_values[i] != power_values[i-1]]
        avg_diff = statistics.mean(diffs) if diffs else 0
        max_diff = max(diffs) if diffs else 0
        print(f"\n--- Data Dynamics & Freshness ---")
        print(f"Total power samples:   {len(power_values)}")
        print(f"Times power updated:   {changes} / {len(power_values)-1} ({changes / (len(power_values)-1) * 100:.1f}% of intervals)")
        print(f"Min Active Power:      {min(power_values):.1f} W")
        print(f"Max Active Power:      {max(power_values):.1f} W")
        print(f"Mean Active Power:     {statistics.mean(power_values):.1f} W")
        print(f"Avg delta on change:   {avg_diff:.1f} W (Max jump: {max_diff:.1f} W)")
    print("="*55)

if __name__ == "__main__":
    asyncio.run(run_benchmark())
