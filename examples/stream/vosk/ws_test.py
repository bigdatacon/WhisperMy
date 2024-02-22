import websockets
import asyncio
import argparse
import sys

async def ws_client(host):
    try:
        async with websockets.connect(host, open_timeout=1) as ws:
            await ws.send('ping')
            if isinstance(await ws.recv(), str):
                print('success')
                sys.exit(0)
    except Exception:
        print('error')
        sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='WS test script')
    parser.add_argument('--host', type=str, default='ws://0.0.0.0:2700', 
                        help='ws host adress')
    args = parser.parse_args()
    asyncio.run(ws_client(args.host))