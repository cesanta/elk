#!/usr/bin/env python3

from collections import defaultdict

type graph = dict[str, list[str]]


def create_graph() -> graph:

    parents: graph = defaultdict(list)

    lines = []
    with open('dotflow', 'r') as f:
        lines = f.readlines()

    i = 0
    while i < len(lines):
        current_line = lines[i]
        if '[' in current_line:
            if not (']' in current_line):
                i += 1
                current_line = current_line.rstrip() + ' ' + lines[i].rstrip()
            parent_name = current_line.split(' [')[0]
            current_parent = parent_name
        else:
            child = current_line.split(' -> ')[1].rstrip()
            parents[current_parent].append(child)

        i += 1

    return parents


is_recursive: dict[str:list[str]] = {}


def find_cycle(seen_nodes: list[str], starting_node: str, g: graph):

    children = g[starting_node]

    if len(children) == 0:
        return

    for v in children:
        if v in seen_nodes:
            is_recursive[v] = seen_nodes.copy()
            return
        seen_nodes.append(v)
        find_cycle(seen_nodes, v, g)
        seen_nodes.remove(v)


g = create_graph()

# for parent, children in graph.items():
#     print(f"{parent}: {', '.join(children)}")

find_cycle(['js_eval'], 'js_eval', g)

for node, cycle in is_recursive.items():

    index_of_node = cycle.index(node)
    if index_of_node == len(cycle) - 1:
        print(f"node {node} calls itself directly")
    else:
        print_cycle = cycle[index_of_node:]
        print(f"{node} is recursive, cycle: {' -> '.join(print_cycle)}")