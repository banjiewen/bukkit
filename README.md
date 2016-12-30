## Bukkit: NIF-based histogram implementations

### bukkit_hdr

`bukkit_hdr` is an implementation of Gil Tene's [HdrHistogram][1] library, based
heavily on the [C implementation][2] by Michael Barker. The
implemenation in this library is heavily simplified - it omits various features
such as coordinated omission support and iterators - and structured for
straightforward concurrent use from Erlang.

Usage is straightforward:

```erlang
%% Create a new histogram.
{ok, Hdr} = bukkit_hdr:new().

%% Update the histogram.
lists:foreach(fun(I) -> bukkit_hdr:update(Hdr, I) end, lists:seq(1, 1000)).

%% Read statistics from the histogram.
bukkit_hdr:read(Hdr).
```

`bukkit_hdr` does not implement any niceties such as a naming service or
"sliding windows"; such features are up to the user to apply as they see fit.

See [bukkit_hdr.erl][3] for documentation on function signatures.

[1]: https://github.com/HdrHistogram/HdrHistogram
[2]: https://github.com/HdrHistogram/HdrHistogram_c
[3]: apps/bukkit_hdr/src/bukkit_hdr.erl
