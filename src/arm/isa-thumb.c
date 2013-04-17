#include "isa-thumb.h"

#include "isa-inlines.h"

static const ThumbInstruction _thumbTable[0x400];

void ThumbStep(struct ARMCore* cpu) {
	uint32_t address = cpu->gprs[ARM_PC];
	cpu->gprs[ARM_PC] = address + WORD_SIZE_THUMB;
	address -= WORD_SIZE_THUMB;
	uint16_t opcode = ((uint16_t*) cpu->memory->activeRegion)[(address & cpu->memory->activeMask) >> 1];
	ThumbInstruction instruction = _thumbTable[opcode >> 6];
	instruction(cpu, opcode);
}

// Instruction definitions
// Beware pre-processor insanity

#define THUMB_ADDITION_S(M, N, D) \
	cpu->cpsr.n = ARM_SIGN(D); \
	cpu->cpsr.z = !(D); \
	cpu->cpsr.c = ARM_CARRY_FROM(M, N, D); \
	cpu->cpsr.v = ARM_V_ADDITION(M, N, D);

#define THUMB_SUBTRACTION_S(M, N, D) \
	cpu->cpsr.n = ARM_SIGN(D); \
	cpu->cpsr.z = !(D); \
	cpu->cpsr.c = ARM_BORROW_FROM(M, N, D); \
	cpu->cpsr.v = ARM_V_SUBTRACTION(M, N, D);

#define THUMB_NEUTRAL_S(M, N, D) \
	cpu->cpsr.n = ARM_SIGN(D); \
	cpu->cpsr.z = !(D);

#define THUMB_ADDITION(D, M, N) \
	int n = N; \
	int m = M; \
	D = M + N; \
	THUMB_ADDITION_S(m, n, D)

#define THUMB_SUBTRACTION(D, M, N) \
	int n = N; \
	int m = M; \
	D = M - N; \
	THUMB_SUBTRACTION_S(m, n, D)

#define APPLY(F, ...) F(__VA_ARGS__)

#define COUNT_1(EMITTER, PREFIX, ...) \
	EMITTER(PREFIX ## 0, 0, __VA_ARGS__) \
	EMITTER(PREFIX ## 1, 1, __VA_ARGS__)

#define COUNT_2(EMITTER, PREFIX, ...) \
	COUNT_1(EMITTER, PREFIX, __VA_ARGS__) \
	EMITTER(PREFIX ## 2, 2, __VA_ARGS__) \
	EMITTER(PREFIX ## 3, 3, __VA_ARGS__)

#define COUNT_3(EMITTER, PREFIX, ...) \
	COUNT_2(EMITTER, PREFIX, __VA_ARGS__) \
	EMITTER(PREFIX ## 4, 4, __VA_ARGS__) \
	EMITTER(PREFIX ## 5, 5, __VA_ARGS__) \
	EMITTER(PREFIX ## 6, 6, __VA_ARGS__) \
	EMITTER(PREFIX ## 7, 7, __VA_ARGS__)

#define COUNT_4(EMITTER, PREFIX, ...) \
	COUNT_3(EMITTER, PREFIX, __VA_ARGS__) \
	EMITTER(PREFIX ## 8, 8, __VA_ARGS__) \
	EMITTER(PREFIX ## 9, 9, __VA_ARGS__) \
	EMITTER(PREFIX ## A, 10, __VA_ARGS__) \
	EMITTER(PREFIX ## B, 11, __VA_ARGS__) \
	EMITTER(PREFIX ## C, 12, __VA_ARGS__) \
	EMITTER(PREFIX ## D, 13, __VA_ARGS__) \
	EMITTER(PREFIX ## E, 14, __VA_ARGS__) \
	EMITTER(PREFIX ## F, 15, __VA_ARGS__)

#define COUNT_5(EMITTER, PREFIX, ...) \
	COUNT_4(EMITTER, PREFIX ## 0, __VA_ARGS__) \
	EMITTER(PREFIX ## 10, 16, __VA_ARGS__) \
	EMITTER(PREFIX ## 11, 17, __VA_ARGS__) \
	EMITTER(PREFIX ## 12, 18, __VA_ARGS__) \
	EMITTER(PREFIX ## 13, 19, __VA_ARGS__) \
	EMITTER(PREFIX ## 14, 20, __VA_ARGS__) \
	EMITTER(PREFIX ## 15, 21, __VA_ARGS__) \
	EMITTER(PREFIX ## 16, 22, __VA_ARGS__) \
	EMITTER(PREFIX ## 17, 23, __VA_ARGS__) \
	EMITTER(PREFIX ## 18, 24, __VA_ARGS__) \
	EMITTER(PREFIX ## 19, 25, __VA_ARGS__) \
	EMITTER(PREFIX ## 1A, 26, __VA_ARGS__) \
	EMITTER(PREFIX ## 1B, 27, __VA_ARGS__) \
	EMITTER(PREFIX ## 1C, 28, __VA_ARGS__) \
	EMITTER(PREFIX ## 1D, 29, __VA_ARGS__) \
	EMITTER(PREFIX ## 1E, 30, __VA_ARGS__) \
	EMITTER(PREFIX ## 1F, 31, __VA_ARGS__) \

#define DEFINE_INSTRUCTION_THUMB(NAME, BODY) \
	static void _ThumbInstruction ## NAME (struct ARMCore* cpu, uint16_t opcode) {  \
		BODY; \
		cpu->cycles += 1 + cpu->memory->activePrefetchCycles16; \
	}

#define DEFINE_IMMEDIATE_5_INSTRUCTION_EX_THUMB(NAME, IMMEDIATE, BODY) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		int immediate = IMMEDIATE; \
		int rd = opcode & 0x0007; \
		int rm = (opcode >> 3) & 0x0007; \
		BODY;)

#define DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB(NAME, BODY) \
	COUNT_5(DEFINE_IMMEDIATE_5_INSTRUCTION_EX_THUMB, NAME ## _, BODY)

DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB(LSL1,
	if (!immediate) {
		cpu->gprs[rd] = cpu->gprs[rm];
	} else {
		cpu->cpsr.c = cpu->gprs[rm] & (1 << (32 - immediate));
		cpu->gprs[rd] = cpu->gprs[rm] << immediate;
	}
	THUMB_NEUTRAL_S( , , cpu->gprs[rd]);)

DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB(LSR1,
	if (!immediate) {
		cpu->cpsr.c = ARM_SIGN(cpu->gprs[rm]);
		cpu->gprs[rd] = 0;
	} else {
		cpu->cpsr.c = cpu->gprs[rm] & (1 << (immediate - 1));
		cpu->gprs[rd] = ((uint32_t) cpu->gprs[rm]) >> immediate;
	}
	THUMB_NEUTRAL_S( , , cpu->gprs[rd]);)

DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB(ASR1, 
	if (!immediate) {
		cpu->cpsr.c = ARM_SIGN(cpu->gprs[rm]);
		if (cpu->cpsr.c) {
			cpu->gprs[rd] = 0xFFFFFFFF;
		} else {
			cpu->gprs[rd] = 0;
		}
	} else {
		cpu->cpsr.c = cpu->gprs[rm] & (1 << (immediate - 1));
		cpu->gprs[rd] = cpu->gprs[rm] >> immediate;
	}
	THUMB_NEUTRAL_S( , , cpu->gprs[rd]);)

DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB(LDR1, cpu->gprs[rd] = cpu->memory->load32(cpu->memory, cpu->gprs[rm] + immediate * 4))
DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB(LDRB1, cpu->gprs[rd] = cpu->memory->loadU8(cpu->memory, cpu->gprs[rm] + immediate))
DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB(LDRH1, cpu->gprs[rd] = cpu->memory->loadU16(cpu->memory, cpu->gprs[rm] + immediate * 2))
DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB(STR1, cpu->memory->store32(cpu->memory, cpu->gprs[rm] + immediate * 4, cpu->gprs[rd]))
DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB(STRB1, cpu->memory->store8(cpu->memory, cpu->gprs[rm] + immediate, cpu->gprs[rd]))
DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB(STRH1, cpu->memory->store16(cpu->memory, cpu->gprs[rm] + immediate * 2, cpu->gprs[rd]))

#define DEFINE_DATA_FORM_1_INSTRUCTION_EX_THUMB(NAME, RM, BODY) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		int rm = RM; \
		int rd = opcode & 0x0007; \
		int rn = (opcode >> 3) & 0x0007; \
		BODY;)

#define DEFINE_DATA_FORM_1_INSTRUCTION_THUMB(NAME, BODY) \
	COUNT_3(DEFINE_DATA_FORM_1_INSTRUCTION_EX_THUMB, NAME ## 3_R, BODY)

DEFINE_DATA_FORM_1_INSTRUCTION_THUMB(ADD, THUMB_ADDITION(cpu->gprs[rd], cpu->gprs[rn], cpu->gprs[rm]))
DEFINE_DATA_FORM_1_INSTRUCTION_THUMB(SUB, THUMB_SUBTRACTION(cpu->gprs[rd], cpu->gprs[rn], cpu->gprs[rm]))

#define DEFINE_DATA_FORM_2_INSTRUCTION_EX_THUMB(NAME, IMMEDIATE, BODY) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		int immediate = IMMEDIATE; \
		int rd = opcode & 0x0007; \
		int rn = (opcode >> 3) & 0x0007; \
		BODY;)

#define DEFINE_DATA_FORM_2_INSTRUCTION_THUMB(NAME, BODY) \
	COUNT_3(DEFINE_DATA_FORM_2_INSTRUCTION_EX_THUMB, NAME ## 1_, BODY)

DEFINE_DATA_FORM_2_INSTRUCTION_THUMB(ADD, THUMB_ADDITION(cpu->gprs[rd], cpu->gprs[rn], immediate))
DEFINE_DATA_FORM_2_INSTRUCTION_THUMB(SUB, THUMB_SUBTRACTION(cpu->gprs[rd], cpu->gprs[rn], immediate))

#define DEFINE_DATA_FORM_3_INSTRUCTION_EX_THUMB(NAME, RD, BODY) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		int rd = RD; \
		int immediate = opcode & 0x00FF; \
		BODY;)

#define DEFINE_DATA_FORM_3_INSTRUCTION_THUMB(NAME, BODY) \
	COUNT_3(DEFINE_DATA_FORM_3_INSTRUCTION_EX_THUMB, NAME ## _R, BODY)

DEFINE_DATA_FORM_3_INSTRUCTION_THUMB(ADD2, THUMB_ADDITION(cpu->gprs[rd], cpu->gprs[rd], immediate))
DEFINE_DATA_FORM_3_INSTRUCTION_THUMB(CMP1, int aluOut = cpu->gprs[rd] - immediate; THUMB_SUBTRACTION_S(cpu->gprs[rd], immediate, aluOut))
DEFINE_DATA_FORM_3_INSTRUCTION_THUMB(MOV1, cpu->gprs[rd] = immediate; THUMB_NEUTRAL_S(, , cpu->gprs[rd]))
DEFINE_DATA_FORM_3_INSTRUCTION_THUMB(SUB2, THUMB_SUBTRACTION(cpu->gprs[rd], cpu->gprs[rd], immediate))

#define DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(NAME, BODY) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		int rd = opcode & 0x0007; \
		int rn = (opcode >> 3) & 0x0007; \
		BODY;)

DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(AND, cpu->gprs[rd] = cpu->gprs[rd] & cpu->gprs[rn]; THUMB_NEUTRAL_S( , , cpu->gprs[rd]))
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(EOR, cpu->gprs[rd] = cpu->gprs[rd] ^ cpu->gprs[rn]; THUMB_NEUTRAL_S( , , cpu->gprs[rd]))
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(LSL2, ARM_STUB)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(LSR2,
	int rs = cpu->gprs[rn] & 0xFF;
	if (rs) {
		if (rs < 32) {
			cpu->cpsr.c = cpu->gprs[rd] & (1 << (rs - 1));
			cpu->gprs[rd] = (uint32_t) cpu->gprs[rd] >> rs;
		} else {
			if (rs > 32) {
				cpu->cpsr.c = 0;
			} else {
				cpu->cpsr.c = ARM_SIGN(cpu->gprs[rd]);
			}
			cpu->gprs[rd] = 0;
		}
	}
	THUMB_NEUTRAL_S( , , cpu->gprs[rd]))

DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(ASR2,
	int rs = cpu->gprs[rn] & 0xFF;
	if (rs) {
		if (rs < 32) {
			cpu->cpsr.c = cpu->gprs[rd] & (1 << (rs - 1));
			cpu->gprs[rd] >>= rs;
		} else {
			cpu->cpsr.c = ARM_SIGN(cpu->gprs[rd]);
			if (cpu->cpsr.c) {
				cpu->gprs[rd] = 0xFFFFFFFF;
			} else {
				cpu->gprs[rd] = 0;
			}
		}
	}
	THUMB_NEUTRAL_S( , , cpu->gprs[rd]))

DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(ADC, ARM_STUB)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(SBC, ARM_STUB)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(ROR, ARM_STUB)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(TST, ARM_STUB)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(NEG, THUMB_SUBTRACTION(cpu->gprs[rd], 0, cpu->gprs[rn]))
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(CMP2, int32_t aluOut = cpu->gprs[rd] - cpu->gprs[rn]; THUMB_SUBTRACTION_S(cpu->gprs[rd], cpu->gprs[rn], aluOut))
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(CMN, ARM_STUB)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(ORR, cpu->gprs[rd] = cpu->gprs[rd] | cpu->gprs[rn]; THUMB_NEUTRAL_S( , , cpu->gprs[rd]))
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(MUL, cpu->gprs[rd] *= cpu->gprs[rn]; THUMB_NEUTRAL_S( , , cpu->gprs[rd]))
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(BIC, cpu->gprs[rd] = cpu->gprs[rd] & ~cpu->gprs[rn]; THUMB_NEUTRAL_S( , , cpu->gprs[rd]))
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(MVN, cpu->gprs[rd] = ~cpu->gprs[rn]; THUMB_NEUTRAL_S( , , cpu->gprs[rd]))

#define DEFINE_INSTRUCTION_WITH_HIGH_EX_THUMB(NAME, H1, H2, BODY) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		int rd = opcode & 0x0007 | H1; \
		int rm = (opcode >> 3) & 0x0007 | H2; \
		BODY;)

#define DEFINE_INSTRUCTION_WITH_HIGH_THUMB(NAME, BODY) \
	DEFINE_INSTRUCTION_WITH_HIGH_EX_THUMB(NAME ## 00, 0, 0, BODY) \
	DEFINE_INSTRUCTION_WITH_HIGH_EX_THUMB(NAME ## 01, 0, 8, BODY) \
	DEFINE_INSTRUCTION_WITH_HIGH_EX_THUMB(NAME ## 10, 8, 0, BODY) \
	DEFINE_INSTRUCTION_WITH_HIGH_EX_THUMB(NAME ## 11, 8, 8, BODY)

DEFINE_INSTRUCTION_WITH_HIGH_THUMB(ADD4, cpu->gprs[rd] += cpu->gprs[rm])
DEFINE_INSTRUCTION_WITH_HIGH_THUMB(CMP3, int32_t aluOut = cpu->gprs[rd] - cpu->gprs[rm]; THUMB_SUBTRACTION_S(cpu->gprs[rd], cpu->gprs[rm], aluOut))
DEFINE_INSTRUCTION_WITH_HIGH_THUMB(MOV3, cpu->gprs[rd] = cpu->gprs[rm])

#define DEFINE_IMMEDIATE_WITH_REGISTER_EX_THUMB(NAME, RD, BODY) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		int rd = RD; \
		int immediate = (opcode & 0x00FF) << 2; \
		BODY;)

#define DEFINE_IMMEDIATE_WITH_REGISTER_THUMB(NAME, BODY) \
	COUNT_3(DEFINE_IMMEDIATE_WITH_REGISTER_EX_THUMB, NAME ## _R, BODY)

DEFINE_IMMEDIATE_WITH_REGISTER_THUMB(LDR3, cpu->gprs[rd] = cpu->memory->load32(cpu->memory, cpu->gprs[ARM_PC] + immediate))
DEFINE_IMMEDIATE_WITH_REGISTER_THUMB(LDR4, cpu->gprs[rd] = cpu->memory->load32(cpu->memory, cpu->gprs[ARM_SP] + immediate))
DEFINE_IMMEDIATE_WITH_REGISTER_THUMB(STR3, cpu->memory->store32(cpu->memory, cpu->gprs[ARM_SP] + immediate, cpu->gprs[rd]))

DEFINE_IMMEDIATE_WITH_REGISTER_THUMB(ADD5, ARM_STUB)
DEFINE_IMMEDIATE_WITH_REGISTER_THUMB(ADD6, cpu->gprs[rd] = cpu->gprs[ARM_SP] + immediate)

#define DEFINE_LOAD_STORE_WITH_REGISTER_EX_THUMB(NAME, RM, BODY) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		int rm = RM; \
		int rd = opcode & 0x0007; \
		int rn = (opcode >> 3) & 0x0007; \
		BODY;)

#define DEFINE_LOAD_STORE_WITH_REGISTER_THUMB(NAME, BODY) \
	COUNT_3(DEFINE_LOAD_STORE_WITH_REGISTER_EX_THUMB, NAME ## _R, BODY)

DEFINE_LOAD_STORE_WITH_REGISTER_THUMB(LDR2, ARM_STUB)
DEFINE_LOAD_STORE_WITH_REGISTER_THUMB(LDRB2, cpu->gprs[rd] = cpu->memory->loadU8(cpu->memory, cpu->gprs[rn] + cpu->gprs[rm]))
DEFINE_LOAD_STORE_WITH_REGISTER_THUMB(LDRH2, cpu->gprs[rd] = cpu->memory->loadU16(cpu->memory, cpu->gprs[rn] + cpu->gprs[rm]))
DEFINE_LOAD_STORE_WITH_REGISTER_THUMB(LDRSB, cpu->gprs[rd] = cpu->memory->load8(cpu->memory, cpu->gprs[rn] + cpu->gprs[rm]))
DEFINE_LOAD_STORE_WITH_REGISTER_THUMB(LDRSH, cpu->gprs[rd] = cpu->memory->load16(cpu->memory, cpu->gprs[rn] + cpu->gprs[rm]))
DEFINE_LOAD_STORE_WITH_REGISTER_THUMB(STR2, ARM_STUB)
DEFINE_LOAD_STORE_WITH_REGISTER_THUMB(STRB2, ARM_STUB)
DEFINE_LOAD_STORE_WITH_REGISTER_THUMB(STRH2, ARM_STUB)

#define DEFINE_LOAD_STORE_MULTIPLE_EX_THUMB(NAME, RS, ADDRESS, LOOP, BODY, OP, PRE_BODY, POST_BODY, WRITEBACK) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		int rn = (opcode >> 8) & 0x000F; \
		int rs = RS; \
		int32_t address = ADDRESS; \
		int m; \
		int i; \
		PRE_BODY; \
		for LOOP { \
			if (rs & m) { \
				BODY; \
				address OP 4; \
			} \
		} \
		POST_BODY; \
		WRITEBACK;)

#define DEFINE_LOAD_STORE_MULTIPLE_THUMB(NAME, BODY, WRITEBACK) \
	COUNT_3(DEFINE_LOAD_STORE_MULTIPLE_EX_THUMB, NAME ## _R, cpu->gprs[rn], (m = 0x01, i = 0; i < 8; m <<= 1, ++i), BODY, +=, , , WRITEBACK)

DEFINE_LOAD_STORE_MULTIPLE_THUMB(LDMIA,
	cpu->gprs[i] = cpu->memory->load32(cpu->memory, address),
	if (!((1 << rn) & rs)) {
		cpu->gprs[rn] = address;
	})

DEFINE_LOAD_STORE_MULTIPLE_THUMB(STMIA,
	cpu->memory->store32(cpu->memory, address, cpu->gprs[i]),
	cpu->gprs[rn] = address)

#define DEFINE_CONDITIONAL_BRANCH_THUMB(COND) \
	DEFINE_INSTRUCTION_THUMB(B ## COND, \
		if (ARM_COND_ ## COND) { \
			int8_t immediate = opcode; \
			cpu->gprs[ARM_PC] += immediate << 1; \
			THUMB_WRITE_PC; \
		})

DEFINE_CONDITIONAL_BRANCH_THUMB(EQ)
DEFINE_CONDITIONAL_BRANCH_THUMB(NE)
DEFINE_CONDITIONAL_BRANCH_THUMB(CS)
DEFINE_CONDITIONAL_BRANCH_THUMB(CC)
DEFINE_CONDITIONAL_BRANCH_THUMB(MI)
DEFINE_CONDITIONAL_BRANCH_THUMB(PL)
DEFINE_CONDITIONAL_BRANCH_THUMB(VS)
DEFINE_CONDITIONAL_BRANCH_THUMB(VC)
DEFINE_CONDITIONAL_BRANCH_THUMB(LS)
DEFINE_CONDITIONAL_BRANCH_THUMB(HI)
DEFINE_CONDITIONAL_BRANCH_THUMB(GE)
DEFINE_CONDITIONAL_BRANCH_THUMB(LT)
DEFINE_CONDITIONAL_BRANCH_THUMB(GT)
DEFINE_CONDITIONAL_BRANCH_THUMB(LE)

DEFINE_INSTRUCTION_THUMB(ADD7, cpu->gprs[ARM_SP] += (opcode & 0x7F) << 2)
DEFINE_INSTRUCTION_THUMB(SUB4, cpu->gprs[ARM_SP] -= (opcode & 0x7F) << 2)

DEFINE_LOAD_STORE_MULTIPLE_EX_THUMB(POP,
	opcode & 0x00FF,
	cpu->gprs[ARM_SP],
	(m = 0x01, i = 0; i < 8; m <<= 1, ++i),
	cpu->gprs[i] = cpu->memory->load32(cpu->memory, address),
	+=,
	, ,
	cpu->gprs[ARM_SP] = address)

DEFINE_LOAD_STORE_MULTIPLE_EX_THUMB(POPR,
	opcode & 0x00FF,
	cpu->gprs[ARM_SP],
	(m = 0x01, i = 0; i < 8; m <<= 1, ++i),
	cpu->gprs[i] = cpu->memory->load32(cpu->memory, address),
	+=,
	,
	cpu->gprs[ARM_PC] = cpu->memory->load32(cpu->memory, address) & 0xFFFFFFFE;
	address += 4;,
	cpu->gprs[ARM_SP] = address;
	THUMB_WRITE_PC;)

DEFINE_LOAD_STORE_MULTIPLE_EX_THUMB(PUSH,
	opcode & 0x00FF,
	cpu->gprs[ARM_SP] - 4,
	(m = 0x80, i = 7; m; m >>= 1, --i),
	cpu->memory->store32(cpu->memory, address, cpu->gprs[i]),
	-=,
	, ,
	cpu->gprs[ARM_SP] = address + 4)

DEFINE_LOAD_STORE_MULTIPLE_EX_THUMB(PUSHR,
	opcode & 0x00FF,
	cpu->gprs[ARM_SP] - 4,
	(m = 0x80, i = 7; m; m >>= 1, --i),
	cpu->memory->store32(cpu->memory, address, cpu->gprs[i]),
	-=,
	cpu->memory->store32(cpu->memory, address, cpu->gprs[ARM_LR]);
	address -= 4;,
	,
	cpu->gprs[ARM_SP] = address + 4)

DEFINE_INSTRUCTION_THUMB(ILL, ARM_STUB)
DEFINE_INSTRUCTION_THUMB(BKPT, ARM_STUB)
DEFINE_INSTRUCTION_THUMB(B,
	int16_t immediate = (opcode & 0x07FF) << 5;
	cpu->gprs[ARM_PC] += (((int32_t) immediate) >> 4);
	THUMB_WRITE_PC;)

DEFINE_INSTRUCTION_THUMB(BL1,
	int16_t immediate = (opcode & 0x07FF) << 5;
	cpu->gprs[ARM_LR] = cpu->gprs[ARM_PC] + (((int32_t) immediate) << 7);)

DEFINE_INSTRUCTION_THUMB(BL2,
	uint16_t immediate = (opcode & 0x07FF) << 1;
	uint32_t pc = cpu->gprs[ARM_PC];
	cpu->gprs[ARM_PC] = cpu->gprs[ARM_LR] + immediate;
	cpu->gprs[ARM_LR] = pc - 1;
	THUMB_WRITE_PC;)

DEFINE_INSTRUCTION_THUMB(BX,
	int rm = (opcode >> 3) & 0xF;
	_ARMSetMode(cpu, cpu->gprs[rm] & 0x00000001);
	int misalign = 0;
	if (rm == ARM_PC) {
		misalign = cpu->gprs[rm] & 0x00000002;
	}
	cpu->gprs[ARM_PC] = cpu->gprs[rm] & 0xFFFFFFFE - misalign;
	if (cpu->executionMode == MODE_THUMB) {
		THUMB_WRITE_PC;
	} else {
		ARM_WRITE_PC;
	})

DEFINE_INSTRUCTION_THUMB(SWI, cpu->board->swi16(cpu->board, opcode & 0xFF))

#define DECLARE_INSTRUCTION_THUMB(EMITTER, NAME) \
	EMITTER ## NAME

#define DECLARE_INSTRUCTION_WITH_HIGH_THUMB(EMITTER, NAME) \
	DECLARE_INSTRUCTION_THUMB(EMITTER, NAME ## 00), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, NAME ## 01), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, NAME ## 10), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, NAME ## 11)

#define DUMMY(X, ...) X,
#define DUMMY_4(...) \
	DUMMY(__VA_ARGS__) \
	DUMMY(__VA_ARGS__) \
	DUMMY(__VA_ARGS__) \
	DUMMY(__VA_ARGS__)

#define DECLARE_THUMB_EMITTER_BLOCK(EMITTER) \
	APPLY(COUNT_5, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, LSL1_)) \
	APPLY(COUNT_5, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, LSR1_)) \
	APPLY(COUNT_5, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, ASR1_)) \
	APPLY(COUNT_3, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, ADD3_R)) \
	APPLY(COUNT_3, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, SUB3_R)) \
	APPLY(COUNT_3, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, ADD1_)) \
	APPLY(COUNT_3, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, SUB1_)) \
	APPLY(COUNT_3, DUMMY_4, DECLARE_INSTRUCTION_THUMB(EMITTER, MOV1_R)) \
	APPLY(COUNT_3, DUMMY_4, DECLARE_INSTRUCTION_THUMB(EMITTER, CMP1_R)) \
	APPLY(COUNT_3, DUMMY_4, DECLARE_INSTRUCTION_THUMB(EMITTER, ADD2_R)) \
	APPLY(COUNT_3, DUMMY_4, DECLARE_INSTRUCTION_THUMB(EMITTER, SUB2_R)) \
	DECLARE_INSTRUCTION_THUMB(EMITTER, AND), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, EOR), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, LSL2), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, LSR2), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, ASR2), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, ADC), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, SBC), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, ROR), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, TST), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, NEG), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, CMP2), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, CMN), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, ORR), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, MUL), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, BIC), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, MVN), \
	DECLARE_INSTRUCTION_WITH_HIGH_THUMB(EMITTER, ADD4), \
	DECLARE_INSTRUCTION_WITH_HIGH_THUMB(EMITTER, CMP3), \
	DECLARE_INSTRUCTION_WITH_HIGH_THUMB(EMITTER, MOV3), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, BX), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, BX), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, ILL), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, ILL), \
	APPLY(COUNT_3, DUMMY_4, DECLARE_INSTRUCTION_THUMB(EMITTER, LDR3_R)) \
	APPLY(COUNT_3, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, STR2_R)) \
	APPLY(COUNT_3, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, STRH2_R)) \
	APPLY(COUNT_3, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, STRB2_R)) \
	APPLY(COUNT_3, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, LDRSB_R)) \
	APPLY(COUNT_3, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, LDR2_R)) \
	APPLY(COUNT_3, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, LDRH2_R)) \
	APPLY(COUNT_3, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, LDRB2_R)) \
	APPLY(COUNT_3, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, LDRSH_R)) \
	APPLY(COUNT_5, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, STR1_)) \
	APPLY(COUNT_5, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, LDR1_)) \
	APPLY(COUNT_5, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, STRB1_)) \
	APPLY(COUNT_5, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, LDRB1_)) \
	APPLY(COUNT_5, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, STRH1_)) \
	APPLY(COUNT_5, DUMMY, DECLARE_INSTRUCTION_THUMB(EMITTER, LDRH1_)) \
	APPLY(COUNT_3, DUMMY_4, DECLARE_INSTRUCTION_THUMB(EMITTER, STR3_R)) \
	APPLY(COUNT_3, DUMMY_4, DECLARE_INSTRUCTION_THUMB(EMITTER, LDR4_R)) \
	APPLY(COUNT_3, DUMMY_4, DECLARE_INSTRUCTION_THUMB(EMITTER, ADD5_R)) \
	APPLY(COUNT_3, DUMMY_4, DECLARE_INSTRUCTION_THUMB(EMITTER, ADD6_R)) \
	DECLARE_INSTRUCTION_THUMB(EMITTER, ADD7), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, ADD7), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, SUB4), \
	DECLARE_INSTRUCTION_THUMB(EMITTER, SUB4), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, ILL)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, ILL)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, ILL)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, PUSH)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, PUSHR)), \
	DO_8(DECLARE_INSTRUCTION_THUMB(EMITTER, ILL)), \
	DO_8(DECLARE_INSTRUCTION_THUMB(EMITTER, ILL)), \
	DO_8(DECLARE_INSTRUCTION_THUMB(EMITTER, ILL)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, POP)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, POPR)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, BKPT)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, ILL)), \
	APPLY(COUNT_3, DUMMY_4, DECLARE_INSTRUCTION_THUMB(EMITTER, STMIA_R)) \
	APPLY(COUNT_3, DUMMY_4, DECLARE_INSTRUCTION_THUMB(EMITTER, LDMIA_R)) \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, BEQ)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, BNE)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, BCS)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, BCC)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, BMI)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, BPL)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, BVS)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, BVC)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, BHI)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, BLS)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, BGE)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, BLT)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, BGT)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, BLE)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, ILL)), \
	DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, SWI)), \
	DO_8(DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, B))), \
	DO_8(DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, ILL))), \
	DO_8(DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, BL1))), \
	DO_8(DO_4(DECLARE_INSTRUCTION_THUMB(EMITTER, BL2))) \

static const ThumbInstruction _thumbTable[0x400] = {
	DECLARE_THUMB_EMITTER_BLOCK(_ThumbInstruction)
};