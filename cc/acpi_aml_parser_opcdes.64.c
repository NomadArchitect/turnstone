/**
 * @file acpi_aml_parser_opcodes.64.c
 * @brief acpi aml opcode parser methods
 */

 #include <acpi/aml.h>

int8_t acpi_aml_parse_op_code_with_cnt(uint16_t oc, uint8_t opcnt, acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed, acpi_aml_object_t* preop){
	uint64_t r_consumed = 0;
	uint8_t idx = 0;
	int8_t res = -1;

	apci_aml_opcode_t* opcode = memory_malloc(sizeof(apci_aml_opcode_t));
	opcode->opcode = oc;

	if(preop != NULL) {
		opcode->operand_count = 1 + opcnt;
		opcode->operands[0] = preop;
		idx = 1;
	} else {
		opcode->operand_count = opcnt;
	}

	for(; idx < opcnt; idx++) {
		uint64_t t_consumed = 0;
		acpi_aml_object_t* op = memory_malloc(sizeof(acpi_aml_object_t));

		if(acpi_aml_parse_one_item(ctx, (void**)&op, &t_consumed) != 0) {
			goto cleanup;
		}

		opcode->operands[idx] = op;
		r_consumed += t_consumed;
	}

	if(acpi_aml_executor_opcode(ctx, opcode) != 0) {
		goto cleanup;
	}

	if(data != NULL) {
		acpi_aml_object_t* resobj = (acpi_aml_object_t*)*data;
		resobj->type = ACPI_AML_OT_OPCODE_EXEC_RETURN;
		resobj->opcode_exec_return = opcode->return_obj;
	}

	if(consumed != NULL) {
		*consumed += r_consumed;
	}

	res = 0;

cleanup:

	for(uint8_t i = 0; i < idx; i++) {
		if(opcode->operands[i] != NULL && opcode->operands[i]->refcount == 0) {
			memory_free(opcode->operands[i]);
		}
	}
	memory_free(opcode);

	return res;
}

#define OPCODEPARSER(num) \
	int8_t acpi_aml_parse_opcnt_ ## num(acpi_aml_parser_context_t * ctx, void** data, uint64_t * consumed){ \
		uint64_t t_consumed = 1; \
		uint8_t oc = *ctx->data; \
		ctx->data++; \
		ctx->remaining--; \
     \
		if(acpi_aml_parse_op_code_with_cnt(oc, num, ctx, data, &t_consumed, NULL) != 0) { \
			return -1; \
		} \
     \
		if(consumed != NULL) { \
			*consumed = t_consumed; \
		} \
     \
		return 0; \
	}

OPCODEPARSER(0);
OPCODEPARSER(1);
OPCODEPARSER(2);
OPCODEPARSER(3);
OPCODEPARSER(4);

#define EXTOPCODEPARSER(num) \
	int8_t acpi_aml_parse_extopcnt_ ## num(acpi_aml_parser_context_t * ctx, void** data, uint64_t * consumed){ \
		uint64_t t_consumed = 1; \
		uint16_t oc = 0x5b00; \
		uint8_t t_oc = *ctx->data; \
		oc |= t_oc; \
		ctx->data++; \
		ctx->remaining--; \
     \
		if(acpi_aml_parse_op_code_with_cnt(oc, num, ctx, data, &t_consumed, NULL) != 0) { \
			return -1; \
		} \
     \
		if(consumed != NULL) { \
			*consumed = t_consumed; \
		} \
     \
		return 0; \
	}

EXTOPCODEPARSER(0);
EXTOPCODEPARSER(1);
EXTOPCODEPARSER(2);
EXTOPCODEPARSER(6);

int8_t acpi_aml_parse_logic_ext(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	uint64_t t_consumed = 0;
	uint16_t oc = 0;

	uint8_t oc_t = *ctx->data;
	ctx->data++;
	ctx->remaining--;
	oc = oc_t;

	oc_t = *ctx->data;
	if(oc_t == ACPI_AML_LEQUAL || oc_t == ACPI_AML_LGREATER || oc_t == ACPI_AML_LLESS) {
		ctx->data++;
		ctx->remaining--;
		t_consumed = 2;
		oc |= ((uint16_t)oc_t) << 8;
		if(acpi_aml_parse_op_code_with_cnt(oc, 2, ctx, data, &t_consumed, NULL) != 0) {
			return -1;
		}
	} else {
		t_consumed = 1;
		if(acpi_aml_parse_op_code_with_cnt(oc, 1, ctx, data, &t_consumed, NULL) != 0) {
			return -1;
		}
	}

	if(consumed != NULL) {
		*consumed = t_consumed;
	}

	return 0;
}

int8_t acpi_aml_parse_op_if(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	uint64_t t_consumed = 0;
	uint64_t r_consumed = 1;
	uint64_t plen;

	ctx->data++;
	ctx->remaining--;

	r_consumed += ctx->remaining;
	plen = acpi_aml_parse_package_length(ctx);
	r_consumed -= ctx->remaining;
	r_consumed += plen;

	t_consumed = 0;
	acpi_aml_object_t* predic = memory_malloc(sizeof(acpi_aml_object_t));

	if(acpi_aml_parse_one_item(ctx, (void**)&predic, &t_consumed) != 0) {
		memory_free(predic);
		return -1;
	}

	plen -= t_consumed;

	uint64_t res = acpi_aml_cast_as_integer(predic);

	if(res != 0) {

		uint64_t old_length = ctx->length;
		uint64_t old_remaining = ctx->remaining;

		ctx->length = plen;
		ctx->remaining = plen;

		if(acpi_aml_parse_all_items(ctx, NULL, NULL) != 0) {
			return -1;
		}

		ctx->length = old_length;
		ctx->remaining = old_remaining - plen;

	} else { // discard if part
		ctx->data += plen;
		ctx->remaining -= plen;
	}

	if(*ctx->data == ACPI_AML_ELSE) {
		if(res != 0) { // discard else when if executed
			// pop else op code
			ctx->data++;
			ctx->remaining--;
			r_consumed++;

			r_consumed += ctx->remaining;
			plen = acpi_aml_parse_package_length(ctx);
			r_consumed -= ctx->remaining;
			r_consumed += plen;

			// discard else part
			ctx->data += plen;
			ctx->remaining -= plen;
		} else { // parse else part
			t_consumed = 0;
			if(acpi_aml_parse_op_else(ctx, data, &t_consumed) != 0) {
				return -1;
			}
			r_consumed += t_consumed;
		}
	}

	if(consumed != NULL) {
		*consumed = r_consumed;
	}

	return 0;
}

int8_t acpi_aml_parse_op_else(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	uint64_t r_consumed = 1;
	uint64_t plen;

	ctx->data++;
	ctx->remaining--;

	r_consumed += ctx->remaining;
	plen = acpi_aml_parse_package_length(ctx);
	r_consumed -= ctx->remaining;
	r_consumed += plen;



	uint64_t old_length = ctx->length;
	uint64_t old_remaining = ctx->remaining;

	ctx->length = plen;
	ctx->remaining = plen;

	if(acpi_aml_parse_all_items(ctx, NULL, NULL) != 0) {
		return -1;
	}

	ctx->length = old_length;
	ctx->remaining = old_remaining - plen;


	if(consumed != NULL) {
		*consumed = r_consumed;
	}

	return 0;
}

int8_t acpi_aml_parse_fatal(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	UNUSED(consumed);

	ctx->data++;
	ctx->remaining--;

	// get fatal type 1 byte
	ctx->fatal_error.type = *ctx->data;
	ctx->data++;
	ctx->remaining--;

	// get fatal code 4 byte
	ctx->fatal_error.type = *((uint32_t*)(ctx->data));
	ctx->data += 4;
	ctx->remaining -= 4;

	acpi_aml_object_t* arg = memory_malloc(sizeof(acpi_aml_object_t));

	if(acpi_aml_parse_one_item(ctx, (void**)&arg, NULL) != 0) {
		memory_free(arg);
		return -1;
	}

	ctx->fatal_error.arg = acpi_aml_cast_as_integer(arg);
	ctx->flags.fatal = 1;

	return -1; // fatal always -1 because it is fatal :)
}

int8_t acpi_aml_parse_op_match(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	uint64_t r_consumed = 1;
	uint64_t t_consumed = 0;
	int8_t res = -1;


	apci_aml_opcode_t* opcode = memory_malloc(sizeof(apci_aml_opcode_t));
	opcode->opcode = *ctx->data;
	opcode->operand_count = 6;

	ctx->data++;
	ctx->remaining--;

	uint8_t idx = 0;

	for(uint8_t i = 0; i < 2; i++) {
		acpi_aml_object_t* op = memory_malloc(sizeof(acpi_aml_object_t));
		t_consumed = 0;
		if(acpi_aml_parse_one_item(ctx, (void**)&op, &t_consumed) != 0) {
			if(op->refcount == 0) {
				memory_free(op);
			}
			return -1;
		}
		r_consumed += t_consumed;
		opcode->operands[idx++] = op;

		acpi_aml_object_t* moc = memory_malloc(sizeof(acpi_aml_object_t));
		t_consumed = 0;
		if(acpi_aml_parse_byte_data(ctx, (void**)&moc, &t_consumed) != 0) {
			memory_free(moc);
			return -1;
		}
		r_consumed += t_consumed;
		opcode->operands[idx++] = moc;
	}

	for(uint8_t i = 0; i < 2; i++) {
		acpi_aml_object_t* op = memory_malloc(sizeof(acpi_aml_object_t));
		t_consumed = 0;
		if(acpi_aml_parse_one_item(ctx, (void**)&op, &t_consumed) != 0) {
			if(op->refcount == 0) {
				memory_free(op);
			}
			return -1;
		}
		r_consumed += t_consumed;
		opcode->operands[idx++] = op;
	}


	if(acpi_aml_executor_opcode(ctx, opcode) != 0) {
		goto cleanup;
	}

	if(data != NULL) {
		acpi_aml_object_t* resobj = (acpi_aml_object_t*)*data;
		resobj->type = ACPI_AML_OT_OPCODE_EXEC_RETURN;
		resobj->opcode_exec_return = opcode->return_obj;
	}

	if(consumed != NULL) {
		*consumed += r_consumed;
	}

	res = 0;

cleanup:

	for(uint8_t i = 0; i < idx; i++) {
		if(opcode->operands[i] != NULL && opcode->operands[i]->refcount == 0) {
			memory_free(opcode->operands[i]);
		}
	}
	memory_free(opcode);

	return res;
}

int8_t acpi_aml_parse_op_while(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	uint64_t t_consumed = 0;
	uint64_t r_consumed = 1;
	uint64_t plen;

	ctx->data++;
	ctx->remaining--;

	r_consumed += ctx->remaining;
	plen = acpi_aml_parse_package_length(ctx);
	r_consumed -= ctx->remaining;
	r_consumed += plen;

	t_consumed = 0;
	acpi_aml_object_t* predic = memory_malloc(sizeof(acpi_aml_object_t));

	if(acpi_aml_parse_one_item(ctx, (void**)&predic, &t_consumed) != 0) {
		memory_free(predic);
		return -1;
	}

	plen -= t_consumed;


	uint64_t old_length = ctx->length;
	uint64_t next_remaining = ctx->remaining - plen;

	uint8_t* old_data = ctx->data;
	uint8_t* next_data = old_data + plen;


	while(acpi_aml_cast_as_integer(predic) != 0) {
		ctx->length = plen;
		ctx->remaining = plen;
		ctx->data = old_data;

		int8_t res = acpi_aml_parse_all_items(ctx, NULL, NULL);

		if(res == -1) {
			if(ctx->flags.while_break == 1) {
				ctx->flags.while_break = 0;
				break; // while loop ended
			}
			return -1; // error at parsing
		}
	}

	ctx->length = old_length;
	ctx->remaining = next_remaining;
	ctx->data = next_data;


	if(consumed != NULL) {
		*consumed += r_consumed;
	}

	return 0;
}