import asyncio
import websockets

async def handler(websocket):
    print("Client connected!")
    try:
        # Read the first message from client
        message = await websocket.recv()
        print(f"Received from client: {message}")
        
        # Send a response!
        await websocket.send("Hello from Python Server! This is a complete text frame.")
        print("Sent data to client. Keeping connection open...")
        
        # Keep connection alive for debugging
        await asyncio.sleep(60) 
    except websockets.exceptions.ConnectionClosed:
        print("Client disconnected.")

async def main():
    async with websockets.serve(handler, "localhost", 8765):
        print("Test WebSocket server running on ws://localhost:8765")
        await asyncio.Future()  # run forever

if __name__ == "__main__":
    asyncio.run(main())
