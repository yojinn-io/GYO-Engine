"""The room capacity the product acceptance analyzers expect (pv6 contract §1).

The only Python definition in acceptance; acceptance_capacity.hpp holds the C++ one.
It is a separate file so the frozen analyzers of batch 07a stay byte-identical.
"""
MAX_PLAYERS = 4
