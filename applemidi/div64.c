
#include <linux/kernel.h>
#include <linux/math64.h>

u64
div64_u64 (u64 dividend, u64 divisor)
{
  u32 high = divisor >> 32;
  u64 quot;

  if (high == 0)
    {
      quot = div_u64 (dividend, divisor);
    }
  else
    {
      int n = 1 + fls (high);
      quot = div_u64 (dividend >> n, divisor >> n);

      if (quot != 0)
	quot--;
      if ((dividend - quot * divisor) >= divisor)
	quot++;
    }

  return quot;
}

s64 div64_s64(s64 dividend, s64 divisor)
{
s64 quot, t;

quot = div64_u64(abs64(dividend), abs64(divisor));
t = (dividend ^ divisor) >> 63;

return (quot ^ t) - t;
}