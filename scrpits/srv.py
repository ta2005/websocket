#!/usr/bin/env python
import asyncio
import websockets

async def echo(websocket):
    async for message in websocket:
        print(f"Received: {message}")
        await websocket.send(message)
        print(f"Echoed: {message}")

async def main():
    async with websockets.serve(echo, "localhost", 8765):
        print("Python Echo Server listening on ws://localhost:8765")
        await asyncio.Future()  # run forever

if __name__ == "__main__":
    asyncio.run(main())
