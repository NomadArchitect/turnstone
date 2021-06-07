/**
 * @file acpi_aml_exec_arrays.64.c
 * @brief acpi aml exec method over arrays methods
 */

#include <acpi/aml_internal.h>
#include <video.h>
#include <strings.h>

int8_t acpi_aml_exec_op_sizeof(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){ \
	acpi_aml_object_t* obj = acpi_aml_get_if_arg_local_obj(ctx, opcode->operands[0], 0, 0);

	if(obj == NULL) {
		ctx->flags.fatal = 1;
		return -1;
	}

	int64_t len = 0;
	int8_t res = -1;

	switch (obj->type) {
	case ACPI_AML_OT_STRING:
		len = strlen(obj->string);
		res = 0;
		break;
	case ACPI_AML_OT_BUFFER:
		len = obj->buffer.buflen;
		res = 0;
		break;
	case ACPI_AML_OT_PACKAGE:
		res = acpi_aml_read_as_integer(ctx, obj->package.pkglen, &len);
		break;
	default:
		break;
	}

	if(res == 0) {
		acpi_aml_object_t* len_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

		if(len_obj == NULL) {
			return -1;
		}

		len_obj->type = ACPI_AML_OT_NUMBER;
		len_obj->number.value = len;
		len_obj->number.bytecnt = ctx->revision == 2 ? 8 : 4;

		opcode->return_obj = len_obj;
	}

	return res;
}

int8_t acpi_aml_exec_findsetbit(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){ \
	acpi_aml_object_t* src = opcode->operands[0];
	acpi_aml_object_t* dst = opcode->operands[1];

	int8_t right = opcode->opcode == ACPI_AML_FINDSETLEFTBIT ? 0 : 1;

	int64_t item;

	if(acpi_aml_read_as_integer(ctx, src, &item) != 0) {
		return -1;
	}

	uint8_t loc = 0;
	int8_t idx = 0;

	if(right) {
		while(item > 0) {
			item >>= idx;
			if(item & 0x01) {
				loc = idx + 1;
				break;
			}
			idx++;
		}
	} else {
		uint64_t test = ctx->revision == 2 ? 0x8000000000000000UL : 0x80000000;
		loc = ctx->revision == 2 ? 64 : 32;
		while(test > 0) {
			if(item & test) {
				break;
			}
			loc--;
			test >>= 1;
		}
	}

	if(acpi_aml_is_null_target(dst) != 0) {
		if(acpi_aml_write_as_integer(ctx, loc, dst) != 0) {
			return -1;
		}
	}


	acpi_aml_object_t* res = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

	if(res == NULL) {
		return -1;
	}

	res->type = ACPI_AML_OT_NUMBER;
	res->number.value = loc;
	res->number.bytecnt = ctx->revision == 2 ? 8 : 4;

	opcode->return_obj = res;


	return 0;
}


#define UNIMPLEXEC(name) \
	int8_t acpi_aml_exec_ ## name(acpi_aml_parser_context_t * ctx, acpi_aml_opcode_t * opcode){ \
		UNUSED(ctx); \
		printf("ACPIAML: FATAL method %s for opcode 0x%04x not implemented\n", #name, opcode->opcode); \
		return -1; \
	}

UNIMPLEXEC(concat);
UNIMPLEXEC(concatres);
UNIMPLEXEC(index);
UNIMPLEXEC(match);
UNIMPLEXEC(mid);