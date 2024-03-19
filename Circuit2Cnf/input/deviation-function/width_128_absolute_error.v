module width_128_absolute_error(a, b, abs_err);
parameter _bit = 128;
input [_bit - 1: 0] a;
input [_bit - 1: 0] b;
output reg [_bit - 1: 0] abs_err;
assign abs_err = (a > b)? (a - b): (b - a);
endmodule
