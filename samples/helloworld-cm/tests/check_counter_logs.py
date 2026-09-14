#!/usr/bin/env python3
"""Validate one fresh run: start server, observe >=3 publishes, start client, then stop both apps."""
import argparse
from pathlib import Path
import re


def check(client_text, server_text):
    events = [int(x) for x in re.findall(r"CounterClient CounterChanged value:\s*(\d+)", client_text)]
    replies = [int(x) for x in re.findall(r"CounterClient GetCounter value:\s*(\d+)", client_text)]
    sent = [int(x) for x in re.findall(r"CounterServer CounterChanged value:\s*(\d+)", server_text)]
    calls = re.findall(r"CounterServer GetCounter value:\s*(\d+)", server_text)
    assert re.search(r"CounterClient subscription:\s*Subscribed\b", client_text), "No successful subscription"
    assert len(events) >= 5, "Need at least five received counter events"
    assert all(a < b for a, b in zip(events, events[1:])), "Received counter must increase (single run, no wrap)"
    assert len(replies) == len(calls) == 1, "Demo should make exactly one successful GetCounter call"
    assert sum(value > replies[0] for value in events) >= 3, "Events must continue after the one method call"
    assert set(events).issubset(sent), "Received values must have been published by server"
    assert sent and sent[0] == 1, "Capture server log from fresh startup"
    assert server_text.index("CounterServer CounterChanged value:") < server_text.index("CounterServer GetCounter value:"), "Server must publish before any GetCounter request"
    assert "CounterClient stopped" in client_text, "Missing client subscription cleanup"
    assert "CounterServer stopped" in server_text, "Missing server StopOfferService cleanup"
    assert not re.search(r"Counter(?:Client|Server).*failed", client_text + server_text), "Counter operation failed"
    print(f"PASS: {len(events)} increasing events ({events[0]}..{events[-1]}), one GetCounter={replies[0]}, independent publishing and cleanup")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("client_log", type=Path)
    parser.add_argument("server_log", type=Path)
    args = parser.parse_args()
    check(args.client_log.read_text(errors="replace"), args.server_log.read_text(errors="replace"))
