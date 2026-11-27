// The ink Plugin's test story for failures: a command no dialogue binds, with
// no fallback, and a command given a list.
LIST colours = red, green
EXTERNAL unbound_command(x)
EXTERNAL paint(colour)

Before the fault. #line:fault_before
* [Call the unbound command #line:fault_unbound]
    ~ unbound_command(1)
    After the fault. #line:fault_after
    -> END
* [Paint with a list #line:fault_list]
    ~ paint(red)
    After the paint. #line:fault_painted
    -> END
