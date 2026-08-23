#include "cmd_forth.hpp"

#include <cstddef>
#include <cstdint>

#include "allocator.hpp"
#include "libc/include/stdio.h"
#include "libc/include/string.h"

namespace blockos::cmd::forth
{

constexpr size_t STACK_SIZE = 1024;

struct Word
{
    Word* prev;
    const char* name;
    size_t name_length;
    uint8_t flags;
    bool is_immediate = false; // explcitly set immediate words (to true)
    bool is_code_array = false;
    bool is_literal = false;
    uint64_t literal_value = 0;
    union
    {
        Word** code_array;
        void (*code)();
    };
    uint16_t code_array_length = 0;
};


bool compiling = false;
constexpr size_t MAX_DEFINITION_LENGTH = 64;
Word* compile_buffer[MAX_DEFINITION_LENGTH];
uint16_t compile_length = 0;
Word* compiling_entry = nullptr; // the new entry currently being built

struct Stack
{
    // stack[stack_head - 1] is the top of the stack
    uint64_t stack[STACK_SIZE] = {0};
    size_t stack_head = 0;

    uint64_t return_stack[STACK_SIZE] = {0};

    void push(uint64_t value)
    {
        if (stack_head == STACK_SIZE)
        {
            // Should fault here
            printf("Stack overflow\n");
            return;
        }
        stack[stack_head++] = value;
    }

    uint64_t pop()
    {
        if (stack_head == 0)
        {
            // Should fault here
            printf("Stack is empty\n");
            return 0;
        }
        return stack[--stack_head];
    }

    uint64_t peek() const
    {
        if (stack_head == 0)
        {
            printf("Nothing to see\n");
            return 0;
        }

        return stack[stack_head - 1];
    }
};

Stack data_stack;
Stack return_stack;
const char* input_position = nullptr;

// PRIMITIVES

// Peek the value at the address on top of the stack and push it onto the stack
void peek()
{
    uint64_t addr = data_stack.pop();
    uint64_t val = *reinterpret_cast<volatile uint64_t*>(addr);
    data_stack.push(val);
}

// Write the value on top of the stack to the address below it
void poke()
{
    uint64_t addr = data_stack.pop();
    uint64_t val = data_stack.pop();
    *reinterpret_cast<volatile uint64_t*>(addr) = val;
}

// Print the top item on the stack
void dot()
{
    uint64_t value = data_stack.pop();
    printf("%lld ", static_cast<long long>(value));
}

// Duplicate the top item on the stack
void dup()
{
    uint64_t top = data_stack.peek();
    data_stack.push(top);
}

// Remove the top item from the stack
void drop()
{
    data_stack.pop();
}

// Swap the top two items on the stack
void swap()
{
    uint64_t a = data_stack.pop();
    uint64_t b = data_stack.pop();
    data_stack.push(a);
    data_stack.push(b);
}

// Duplicate the second item on the stack and push it on top of the stack
void over()
{
    uint64_t a = data_stack.pop();
    uint64_t b = data_stack.pop();

    data_stack.push(b);
    data_stack.push(a);
    data_stack.push(b);
}

// Add the top two items on the stack
void add()
{
    uint64_t a = data_stack.pop();
    uint64_t b = data_stack.pop();
    data_stack.push(b + a);
}

// Subtract the top item from the second item on the stack
void sub()
{
    uint64_t a = data_stack.pop();
    uint64_t b = data_stack.pop();
    data_stack.push(b - a);
}

// Multiply the top two items on the stack
void mul()
{
    uint64_t a = data_stack.pop();
    uint64_t b = data_stack.pop();
    data_stack.push(b * a);
}

// Divide the second item on the stack by the top item
void div()
{
    uint64_t a = data_stack.pop();
    uint64_t b = data_stack.pop();

    if (a == 0)
    {
        printf("Division by zero\n");
        data_stack.push(0);
        return;
    }

    data_stack.push(b / a);
}

bool should_exit = false;
void bye()
{
    should_exit = true;
}

// Remember to update links when adding new words
Word peek_word = {.prev = nullptr, .name = "@", .name_length = 1, .flags = 0, .code = peek};
Word poke_word = {.prev = &peek_word, .name = "!", .name_length = 1, .flags = 0, .code = poke};
Word dot_word = {.prev = &poke_word, .name = ".", .name_length = 1, .flags = 0, .code = dot};
Word dup_word = {.prev = &dot_word, .name = "dup", .name_length = 3, .flags = 0, .code = dup};
Word drop_word = {.prev = &dup_word, .name = "drop", .name_length = 4, .flags = 0, .code = drop};
Word swap_word = {.prev = &drop_word, .name = "swap", .name_length = 4, .flags = 0, .code = swap};
Word over_word = {.prev = &swap_word, .name = "over", .name_length = 4, .flags = 0, .code = over};
Word add_word = {.prev = &over_word, .name = "+", .name_length = 1, .flags = 0, .code = add};
Word sub_word = {.prev = &add_word, .name = "-", .name_length = 1, .flags = 0, .code = sub};
Word mul_word = {.prev = &sub_word, .name = "*", .name_length = 1, .flags = 0, .code = mul};
Word div_word = {.prev = &mul_word, .name = "/", .name_length = 1, .flags = 0, .code = div};
Word bye_word = {.prev = &div_word, .name = "bye", .name_length = 3, .flags = 0, .code = bye};

// COMPILE WORDS

// Start compiling a new word definition
bool next_token(const char* input_position, const char** token, uint8_t* token_length, const char** new_input_position);
extern Word* dictionary_head;

void colon()
{
    const char* token;
    uint8_t token_length;

    if (compiling)
    {
        printf("Already compiling\n");
        return;
    }

    if (!next_token(input_position, &token, &token_length, &input_position))
    {
        printf("Expected word name after ':'\n");
        return;
    }

    char* name = new char[token_length + 1];
    memcpy(name, token, token_length);
    name[token_length] = '\0';

    Word* word = new Word;
    word->name = name;
    word->name_length = token_length;
    word->flags = 0;
    word->prev = dictionary_head;

    compiling_entry = word;
    compile_length = 0;
    compiling = true;
}

// Finish compiling a word definition and add it to the dictionary
void semicolon()
{
    if (!compiling)
    {
        printf("Not compiling\n");
        return;
    }

    Word** code_array = new Word*[compile_length + 1];
    for (uint16_t i = 0; i < compile_length; i++)
    {
        code_array[i] = compile_buffer[i];
    }

    compiling_entry->code_array = code_array;
    compiling_entry->code_array_length = compile_length;
    compiling_entry->is_code_array = true;

    dictionary_head = compiling_entry;
    compiling_entry = nullptr;
    compile_length = 0;
    compiling = false;
}

// Remember to update links when adding new words
Word colon_word = {.prev = &bye_word, .name = ":", .name_length = 1, .flags = 0, .is_immediate = true, .code = colon};
Word semicolon_word = {.prev = &colon_word, .name = ";", .name_length = 1, .flags = 0, .is_immediate = true, .code = semicolon};

Word* dictionary_head = &semicolon_word;

// HELPERS

void abort_compile()
{
    compiling = false;
    compiling_entry = nullptr;
    compile_length = 0;
}

Word* find_word(const char* token, uint8_t token_length)
{
    for (Word* word = dictionary_head; word != nullptr; word = word->prev)
    {
        if (word->name_length == token_length && memcmp(word->name, token, token_length) == 0)
        {
            return word;
        }
    }

    return nullptr;
}

uint64_t parse_number(const char* token, uint8_t token_length, bool* is_number)
{
    uint64_t out = 0;
    bool is_negative = false;
    uint8_t i = 0;

    if (token_length > 0 && *token == '-')
    {
        is_negative = true;
        i = 1;
    }

    if (token_length == 0 || (is_negative && token_length == 1))
    {
        *is_number = false;
        return 0;
    }

    for (; i < token_length; i++)
    {
        const char ch = token[i];
        if (ch < '0' || ch > '9')
        {
            *is_number = false;
            return 0;
        }
        out = out * 10 + static_cast<uint64_t>(ch - '0');
    }

    *is_number = true;
    return is_negative ? -out : out;
}

bool next_token(const char* position, const char** token, uint8_t* token_length, const char** position_out)
{
    auto is_space = [](char c) -> bool
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };

    if (is_space(*position) || *position == '\0')
    {
        while (is_space(*position))
            position++;
        if (*position == '\0') return false;
    }

    const char* start = position;
    uint8_t length = 0;
    while (!is_space(*position) && *position != '\0')
    {
        position++;
        length++;
    }

    *token = start;
    *token_length = length;
    *position_out = position;

    return true;
}

// CORE INTERPRETER

void inner_interpreter(Word* word)
{
    if (word->is_literal)
    {
        data_stack.push(word->literal_value);
        return;
    }

    if (word->is_code_array)
    {
        for (uint16_t i = 0; i < word->code_array_length; i++)
        {
            inner_interpreter(word->code_array[i]);
        }
    }
    else
        word->code();
}

void outer_interpreter(const char* line)
{
    input_position = line;

    while (true)
    {
        const char* token;
        uint8_t token_length;

        if (!next_token(input_position, &token, &token_length, &input_position))
        {
            break;
        }

        Word* word = find_word(token, token_length);
        if (word != nullptr)
        {
            if (compiling && !word->is_immediate)
            {
                if (compile_length == MAX_DEFINITION_LENGTH)
                {
                    printf("Definition too long\n");
                    abort_compile();
                    return;
                }

                compile_buffer[compile_length++] = word;
                continue;
            }

            inner_interpreter(word);

            if (should_exit)
                return;

            continue;
        }

        bool is_number = false;
        uint64_t value = parse_number(token, token_length, &is_number);
        if (is_number)
        {
            if (!compiling)
            {
                data_stack.push(value);
                continue;
            }

            if (compile_length == MAX_DEFINITION_LENGTH)
            {
                printf("Definition too long\n");
                abort_compile();
                return;
            }

            Word* literal = new Word;
            literal->prev = nullptr;
            literal->name = nullptr;
            literal->name_length = 0;
            literal->flags = 0;
            literal->is_literal = true;
            literal->literal_value = value;
            literal->code = nullptr;

            compile_buffer[compile_length++] = literal;
            continue;
        }

        printf("? ");
        printf("%s", token);
        printf("\n");

        abort_compile();
        return;
    }
}

bool active = false;
constexpr size_t LINE_MAX = 256;

bool command(const Args& args)
{
    size_t first = 0;

    if (!active)
    {
        if (args.count == 0 || strcmp(args.at(0), "forth") != 0)
            return false;

        active = true;
        should_exit = false;
        abort_compile();

        printf("forth: 'bye' to exit. Lines must be under 256 characters.\n");
        first = 1;
    }

    char line[LINE_MAX];
    size_t length = 0;

    for (size_t i = first; i < args.count; i++)
    {
        const char* token = args.at(i);

        while (*token != '\0' && length + 1 < LINE_MAX)
            line[length++] = *token++;

        if (length + 1 < LINE_MAX)
            line[length++] = ' ';
    }

    line[length] = '\0';

    outer_interpreter(line);

    if (should_exit)
    {
        active = false;
        printf("bye\n");
        return true;
    }

    printf("%s\n", compiling ? " compiling..." : " ok");

    return true;
}

} // namespace blockos::cmd::forth