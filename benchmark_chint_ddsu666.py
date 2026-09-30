import asyncio
import json
import time
import csv
import sys
import statistics
import websockets

CHINT_WS_URL = "ws://10.0.0.197:81/"
STREAM_DURATION_SEC = 15
RPC_DURATION_SEC = 15
RPC_INTERVAL_SEC = 0.050  # 50ms (20 Hz query rate to test limits!)
CSV_FILENAME = "chint_ddsu666_benchmark.csv"

async def benchmark_stream(ws):
    print("\n" + "="*60)
    print(" 1. BENCHMARKING PASSIVE WEBSOCKET BROADCAST STREAM (15s)")
    print("="*60)
    print("Listening to real-time push frames broadcast by the XIAO ESP32-S3...")

    samples = []
    start_time = time.time()
    end_time = start_time + STREAM_DURATION_SEC
    last_arrival = None

    while time.time() < end_time:
        try:
            raw_msg = await asyncio.wait_for(ws.recv(), timeout=1.0)
            now = time.perf_counter()
            inter_arrival_ms = (now - last_arrival) * 1000.0 if last_arrival else None
            last_arrival = now

            msg = json.loads(raw_msg)
            # Accept either clean telemetry or NotifyStatus
            if msg.get("type") == "telemetry":
                samples.append({
                    "time": time.time(),
                    "inter_arrival_ms": inter_arrival_ms,
                    "watts": msg.get("watts"),
                    "volts": msg.get("volts"),
                    "amps": msg.get("amps"),
                    "pf": msg.get("pf"),
                    "freq": msg.get("freq"),
                    "poll_hz": msg.get("poll_hz"),
                    "meter_latency_ms": msg.get("latency_ms"),
                })
                sys.stdout.write(f"\rCaptured {len(samples)} frames | Power: {msg.get('watts'):.1f}W | Modbus Hz: {msg.get('poll_hz'):.1f} Hz | Meter Latency: {msg.get('latency_ms'):.1f}ms")
                sys.stdout.flush()
        except asyncio.TimeoutError:
            print("\nWarning: Stream receive timeout!")
            break

    elapsed = time.time() - start_time
    print(f"\nCompleted {len(samples)} stream frames in {elapsed:.2f}s ({len(samples)/elapsed:.1f} Hz)")

    delays = [s["inter_arrival_ms"] for s in samples if s["inter_arrival_ms"] is not None]
    if delays:
        delays.sort()
        print(f"\n--- Broadcast Arrival Interval Stats ---")
        print(f"Effective Broadcast Rate: {len(samples)/elapsed:.2f} frames/sec")
        print(f"Min interval:            {min(delays):.2f} ms")
        print(f"Median interval:         {statistics.median(delays):.2f} ms")
        print(f"Mean interval:           {statistics.mean(delays):.2f} ms")
        print(f"P95 interval:            {delays[int(len(delays)*0.95)]:.2f} ms")
        print(f"Max interval:            {max(delays):.2f} ms")
        print(f"Jitter (StdDev):         {statistics.stdev(delays):.2f} ms")

    return samples

async def benchmark_rpc(ws):
    print("\n" + "="*60)
    print(" 2. BENCHMARKING ACTIVE RPC REQUEST-RESPONSE (50ms interval, 15s)")
    print("="*60)
    print("Querying EM1.GetStatus RPC every 50ms (20 Hz polling)...")

    records = []
    start_time = time.time()
    end_time = start_time + RPC_DURATION_SEC
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
            # Find the RPC response matching this msg_id (skipping broadcast frames if interleaved)
            resp = None
            while True:
                raw = await asyncio.wait_for(ws.recv(), timeout=0.3)
                parsed = json.loads(raw)
                if parsed.get("id") == msg_id:
                    resp = parsed
                    break

            rtt_ms = (time.perf_counter() - t0) * 1000.0
            res = resp.get("result", {})
            em1 = res.get("em1:0", {})
            act_power = em1.get("act_power", 0.0)
            voltage = em1.get("voltage", 0.0)
            current = em1.get("current", 0.0)
            pf = em1.get("pf", 0.0)
            freq = em1.get("freq", 0.0)
            success = True
            error_str = ""
        except asyncio.TimeoutError:
            rtt_ms = (time.perf_counter() - t0) * 1000.0
            act_power = voltage = current = pf = freq = None
            success = False
            error_str = "TIMEOUT"
        except Exception as e:
            rtt_ms = (time.perf_counter() - t0) * 1000.0
            act_power = voltage = current = pf = freq = None
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
            "pf": pf,
            "freq": freq,
            "error": error_str
        })

        if msg_id % 25 == 0:
            sys.stdout.write(f"\rQueries: {msg_id} | Latest RTT: {rtt_ms:.2f}ms | Power: {act_power}W")
            sys.stdout.flush()

        next_tick += RPC_INTERVAL_SEC
        sleep_dur = next_tick - time.time()
        if sleep_dur > 0:
            await asyncio.sleep(sleep_dur)
        else:
            next_tick = time.time()

    elapsed = time.time() - start_time
    print(f"\nCompleted {len(records)} RPC requests in {elapsed:.2f}s")

    # Analysis
    successful_rtts = [r["rtt_ms"] for r in records if r["success"]]
    failures = len(records) - len(successful_rtts)

    print("\n" + "="*60)
    print("        CHINT DDSU666 WEBSOCKET RPC BENCHMARK REPORT")
    print("="*60)
    print(f"Total Duration:        {elapsed:.2f} seconds")
    print(f"Total RPC Queries:     {len(records)}")
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

    return records

async def main():
    print(f"Connecting to Chint DDSU666 WebSocket: {CHINT_WS_URL} ...")
    try:
        ws = await websockets.connect(CHINT_WS_URL, close_timeout=2)
    except Exception as e:
        print(f"Connection failed: {e}")
        return

    print("Connected successfully!")
    stream_samples = await benchmark_stream(ws)
    rpc_records = await benchmark_rpc(ws)
    await ws.close()

    # Save to CSV
    with open(CSV_FILENAME, mode="w", newline="") as f:
        fieldnames = ["timestamp", "elapsed_sec", "msg_id", "rtt_ms", "success", "voltage", "current", "act_power", "pf", "freq", "error"]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for r in rpc_records:
            writer.writerow(r)
    print(f"\nBenchmark data written to {CSV_FILENAME}")

if __name__ == "__main__":
    asyncio.run(main())
