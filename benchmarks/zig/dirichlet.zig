// -O ReleaseSmall

const std = @import("std");

const N: comptime_int = 150000000;

var spf: [N + 1]i32 = std.mem.zeroes([N + 1]i32);
var d: [N + 1]i32 = std.mem.zeroes([N + 1]i32);
var e: [N + 1]i32 = std.mem.zeroes([N + 1]i32);
var primes: [N + 1]i32 = undefined;

pub fn main() void {
    var pcount: usize = 0;

    d[1] = 1;
    e[1] = 0;
    spf[1] = 0;
    for (2..N + 1) |i| {
        if (spf[i] == 0) {
            spf[i] = @intCast(i);
            d[i] = 2;
            e[i] = 1;
            primes[pcount] = @intCast(i);
            pcount += 1;
        } else {
            const p: i32 = spf[i];
            const j: usize = @divFloor(i, @as(usize, @intCast(p)));
            if (spf[j] == p) {
                e[i] = e[j] + 1;
                d[i] = @divFloor(d[j] * (e[i] + 1), e[j] + 1);
            } else {
                e[i] = 1;
                d[i] = d[j] * 2;
            }
        }

        for (0..pcount) |j_index| {
            const p = primes[j_index];
            if (p > spf[i] or @as(i64, @intCast(p)) * @as(i64, @intCast(i)) > N)
                break;
            spf[@as(usize, @intCast(p)) * i] = p;
        }
    }
    var total: u64 = 0;
    for (1..N + 1) |i| {
        total += @intCast(d[i]);
    }
    std.debug.print("{d}\n", .{total});
}
