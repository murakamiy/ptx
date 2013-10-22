#define PT3_BUS_CMD_MAX   4096
#define PT3_BUS_CMD_ADDR0 4096 + 0
#define PT3_BUS_CMD_ADDR1 4096 + 2042

struct pt3_bus {
	u32 read_addr, cmd_addr, cmd_count, cmd_pos, buf_pos, buf_size;
	u8 cmd_tmp, cmds[PT3_BUS_CMD_MAX], *buf;
};

enum pt3_bus_cmd {
	I_END,
	I_ADDRESS,
	I_CLOCK_L,
	I_CLOCK_H,
	I_DATA_L,
	I_DATA_H,
	I_RESET,
	I_SLEEP,
	I_DATA_L_NOP  = 0x08,
	I_DATA_H_NOP  = 0x0c,
	I_DATA_H_READ = 0x0d,
	I_DATA_H_ACK0 = 0x0e,
	I_DATA_H_ACK1 = 0x0f,
};

static void pt3_bus_add_cmd(struct pt3_bus *bus, enum pt3_bus_cmd cmd)
{
	if ((bus->cmd_count % 2) == 0) {
		bus->cmd_tmp = cmd;
	} else {
		bus->cmd_tmp |= cmd << 4;
	}

	if (bus->cmd_count % 2) {
		bus->cmds[bus->cmd_pos] = bus->cmd_tmp;
		bus->cmd_pos++;
		if (bus->cmd_pos >= sizeof(bus->cmds)) {
			PT3_PRINTK(KERN_ALERT, "bus->cmds is overflow\n");
			bus->cmd_pos = 0;
		}
	}
	bus->cmd_count++;
}

u8 pt3_bus_data1(struct pt3_bus *bus, u32 index)
{
	if (unlikely(!bus->buf)) {
		PT3_PRINTK(KERN_ALERT, "buf is not ready.\n");
		return 0;
	}
	if (unlikely(bus->buf_size < index + 1)) {
		PT3_PRINTK(KERN_ALERT, "buf does not have enough size. buf_size=%d\n",
				bus->buf_size);
		return 0;
	}

	return bus->buf[index];
}

void pt3_bus_start(struct pt3_bus *bus)
{
	pt3_bus_add_cmd(bus, I_DATA_H);
	pt3_bus_add_cmd(bus, I_CLOCK_H);
	pt3_bus_add_cmd(bus, I_DATA_L);
	pt3_bus_add_cmd(bus, I_CLOCK_L);
}

void pt3_bus_stop(struct pt3_bus *bus)
{
	pt3_bus_add_cmd(bus, I_DATA_L);
	pt3_bus_add_cmd(bus, I_CLOCK_H);
	pt3_bus_add_cmd(bus, I_DATA_H);
}

void pt3_bus_write(struct pt3_bus *bus, const u8 *data, u32 size)
{
	u32 i, j;
	u8 byte;

	for (i = 0; i < size; i++) {
		byte = data[i];
		for (j = 0; j < 8; j++) {
			pt3_bus_add_cmd(bus, PT3_SHIFT_MASK(byte, 7 - j, 1) ? I_DATA_H_NOP : I_DATA_L_NOP);
		}
		pt3_bus_add_cmd(bus, I_DATA_H_ACK0);
	}
}

u32 pt3_bus_read(struct pt3_bus *bus, u8 *data, u32 size)
{
	u32 i, j;
	u32 index;

	for (i = 0; i < size; i++) {
		for (j = 0; j < 8; j++) {
			pt3_bus_add_cmd(bus, I_DATA_H_READ);
		}

		if (i == (size - 1))
			pt3_bus_add_cmd(bus, I_DATA_H_NOP);
		else
			pt3_bus_add_cmd(bus, I_DATA_L_NOP);
	}
	index = bus->read_addr;
	bus->read_addr += size;
	if (likely(bus->buf == NULL)) {
		bus->buf = data;
		bus->buf_pos = 0;
		bus->buf_size = size;
	} else
		PT3_PRINTK(KERN_ALERT, "bus read buf already exists.\n");

	return index;
}

void pt3_bus_push_read_data(struct pt3_bus *bus, u8 data)
{
	if (unlikely(bus->buf)) {
		if (bus->buf_pos >= bus->buf_size) {
			PT3_PRINTK(KERN_ALERT, "buffer over run. pos=%d\n", bus->buf_pos);
			bus->buf_pos = 0;
		}
		bus->buf[bus->buf_pos] = data;
		bus->buf_pos++;
	}
}

void pt3_bus_sleep(struct pt3_bus *bus, u32 ms)
{
	u32 i;
	for (i = 0; i< ms; i++)
		pt3_bus_add_cmd(bus, I_SLEEP);
}

void pt3_bus_end(struct pt3_bus *bus)
{
	pt3_bus_add_cmd(bus, I_END);

	if (bus->cmd_count % 2)
		pt3_bus_add_cmd(bus, I_END);
}

void pt3_bus_reset(struct pt3_bus *bus)
{
	pt3_bus_add_cmd(bus, I_RESET);
}

