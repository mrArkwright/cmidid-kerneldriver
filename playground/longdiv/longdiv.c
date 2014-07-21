#include <linux/module.h>
#include <asm/div64.h>

MODULE_AUTHOR("Michael Opitz <michael.w.opitz@gmail.com>");
MODULE_DESCRIPTION("Linux kernel 64bit division test.");
MODULE_LICENSE("GPL");

static int __init longdiv_init(void);
static void __exit longdiv_exit(void);

module_init(longdiv_init);
module_exit(longdiv_exit);

/* power */
static u64 power(u64 base, u64 exponent)
{
	u64 last;
	u64 result = base;

	if (exponent == 0)
		return 1;

	while (--exponent > 0) {
		last = result;
		result *= base;

		if (result < last)
			printk(KERN_ALERT "[%s:%s:%d] Overflow: %llu * %llu\n",
			       __FILE__, __func__, __LINE__, last, base);
	}

	return result;
}

/* factorial */
static u64 fac(u64 n)
{
	u64 last;
	u64 result = n;

	if (result == 0)
		return 1;

	while (--n > 0) {
		last = result;
		result *= n;

		if (result < last)
			printk(KERN_ALERT "[%s:%s:%d] Overflow: %llu * %llu\n",
			       __FILE__, __func__, __LINE__, last, n);
	}

	return result;
}

/* Calculates e^z with some series stuff... */
static u64 e_tothe_z(u64 z)
{
	u64 last;
	u64 k = 0, result = 0;

	if (z == 0)
		return 1;

	while (true) {
		last = result;
		result += div64_u64(power(z, k), fac(k));

		if (result <= last)
			return last;

		k++;
	}

}

static int __init longdiv_init(void)
{
	u64 a, b, result;

	printk(KERN_INFO "Starting 64 bit division test.\n");

	a = 3, b = 10;

	result = div64_u64(a, b);

	printk(KERN_INFO "%llu / %llu = %llu\n", a, b, result);

	result = a * b;

	printk(KERN_INFO "%llu * %llu = %llu\n", a, b, result);

	printk(KERN_INFO "%llu ^ %llu = %llu\n", a, b, power(a, b));

	a = 16;

	printk(KERN_INFO "%llu! = %llu\n", a, fac(a));

	a = 20;

	printk(KERN_INFO "e ^ %llu = %llu\n", a, e_tothe_z(a));

	return 0;
}

static void __exit longdiv_exit(void)
{
	printk(KERN_INFO "64 bit division test finished.\n");
}
