/* Implementation of the LLVM's example Kalidascope Language */

/*
Example of the language:

# Compute the x'th fibonacci number.
def fib(x)
    if x < 3 then
        1
    else
        fib(x-1)+fib(x-2);

# This expression will compute the 40th number.
fib(40);

*/

/* 
Beggining of the Lexer

The lexer is responsible for taking the input string and 
breaking it up into a stream of tokens.

*/

// The lexer returns tokens [0 - 255] is it is an unknown 
// character, otherwise one of these for known things.

#include <string>
#include <vector>
#include <memory>
#include <map>

using namespace std;

enum Token {
    tok_eof = -1,

    // Commands
    tok_def = -2,
    tok_extern = -3,

    // Primary
    tok_identifier = -4,
    tok_number = -5,
};

static string IdentifierStr; // Filled in if tok_identiier is called (track the name)
static double NumVal; // Filled in if tok_number is called (tracks the value)


#include <cctype> // Add missing include for the isalpha and isalnum functions
#include <cstdio> // Add missing include for the fprintf function
#include <cstdlib> // Add missing include for the strtod function
#include <string> // Add missing include for the string type
#include <memory> // Add missing include for the unique_ptr type
#include <vector> // Add missing include for the vector type

// Returns the next token from the standard input
static int gettok() {
    static int LastChar = ' '; // Initialized to value to ignore (White space)

    while(isspace(LastChar)){
        LastChar = getchar();
    }
    if(isalpha(LastChar)){ // We know it's some sort of user defined thing
        IdentifierStr = LastChar;
        while (isalnum(LastChar = getchar())) { // This parses the word we are on and decides the action based on what it is
            IdentifierStr += LastChar;
        }
        if(IdentifierStr == "def") { // If the word is def, we are working to define a function. Return tok_def
            return tok_def;
        }
        if(IdentifierStr == "extern") { // If the word is extern, we are working to use an extern resource. Return tok_extern
            return tok_extern;
        }
        return tok_identifier; // Last case is that we are defining some variable
    }
    if(isdigit(LastChar) || LastChar == '.'){
        std::string NumStr;
        bool decimal = false; // Used to check for double .
        do {
            NumStr += LastChar;
            if(!isdigit(LastChar)) {
                decimal = true;
            }
            LastChar = getchar();
            while(!isdigit(LastChar) && decimal) {
                LastChar = getchar();
            }
        } while (isdigit(LastChar) || LastChar == '.');
        NumVal = strtod(NumStr.c_str(), 0); // Converts to decimal
        return tok_number;
    }
    if(LastChar == '#') { // It's comment, keep reading characters until the next line
        do {
            LastChar = getchar();
        } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
        if(LastChar != EOF){
            return gettok();
        }
    }
    if(LastChar == EOF) { // We are done reading
        return tok_eof;
    }
    int ThisChar = LastChar; // Return a value between 0 and 255
    LastChar = getchar();
    return ThisChar;
}


/*
This code is for an Abstract Syntax Tree (AST). We have the base class,
ExprAST, which is meant as a template for how to design our classes.
We also have other ASTs, which store information based on their relavancy.
For example, based on the token recieved, we have ASTs to store numbers,
binary operations, variables, and functions.
*/


class ExprAST {
public:
    virtual ~ExprAST() = default;
};

// Defines a number
class NumberExprAST : public ExprAST {
    double Val;

public:
    NumberExprAST(double Val) : Val(Val) {}
};

// Defines the variable name
class VariableExprAST : public ExprAST {
    std::string Name;

public:
    VariableExprAST(const std::string &Name) : Name(Name) {}
};

// Defines an expression
class BinaryExprAST : public ExprAST {
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;

public:
    BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS) : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
};

// Defines a function CALL
class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;

public:
    CallExprAST(const std::string &Callee, std::vector<std::unique_ptr<ExprAST>> Args) : Callee(Callee), Args(std::move(Args)) {}
};

// Defines the 'ProtoType' for a function; the name and its argument names

class PrototypeAST : public ExprAST {
    std::string Name;
    std::vector<std::string> Args;

public:
    PrototypeAST(const std::string &Name, std::vector<std::string> Args) : Name(Name), Args(std::move(Args)) {}

    const std::string & getName() const {
        return Name;
    }
};

// Defines what the function does

class FunctionAST {
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;

public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto, std::unique_ptr<ExprAST> Body) : Proto(std::move(Proto)), Body(std::move(Body)) {}
};



/*
This part defines the parser; we need a way to convert the tokens we 
receive and the file we are lexing and transfer the info into the AST

For example, given 'x + y', we need to do something like:

auto LHS = std::make_unique<VariableExprAST>("x");
auto RHS = std::make_unique<VariableExprAST>("y");
auto Result = std::make_unique<BinaryExprAST>('+', std::move(LHS),
                                              std::move(RHS));

To generate this code, we can do the following
*/

// Goes through the tokens one by one and allows us to look one token
// ahead at what the lexer will return.

static int CurTok;
static int getNextToken(){
    CurTok = gettok();
    return CurTok;
}

// Error handling functions

std::unique_ptr<ExprAST> LogError(const char *Str) {
    fprintf(stderr, "Error %s\n", Str);
    return nullptr;
}

static std::unique_ptr<ExprAST> ParseExpression();


std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
    LogError(Str);
    return nullptr;
}

static std::unique_ptr<ExprAST> ParseNumberExpr() {
    auto Result = std::make_unique<NumberExprAST>(NumVal);
    getNextToken();
    return std::move(Result);
}

/*

This may be confusing for an idiot (Like myself), so let's 
walk through it. When we read a '(', we call the function above.
Afterwards, we will 'eat' the token/go to the next character. We then
go on to parse the incoming expression, which will end with CurTok having
and end state that is either a ')' or something else. If it's not ')',
throw an error. Otherwise, move on to the next token and return the 
expression we parsed.

*/

static std::unique_ptr<ExprAST> ParseParenExpr() {
  getNextToken(); // eat (.
  auto V = ParseExpression();
  if (!V)
    return nullptr;

  if (CurTok != ')')
    return LogError("expected ')'");
  getNextToken(); // eat ).
  return V;
}



// This handles variable references and function calls

/*

LMAO more explanations you fool

We start off with storing our IdentifierStr in IdName. This is the name
of our variable or function; the next few logic statements decide whether
we are making a function or a variable.

We eat the next token into CurTok and examine it. If the token is not
a '(', we have a variable and should just return a Variable AST. Otherwise,
we will move on to parsing the function and its arguments. 

If there are arguments (We don't have an ending ')'), we should push
these arguments into the Args vector (keeps track of the arguments).
if for some reason we have an empty argument, we return a nullptr.
We also return an error message if the function is not well defined.
Otherwise, we continue to add arguments until we reach a ')'. We eat
it and return a unique_ptr to a CallAST. The CallAST is instantiated
with the name of the function (IdName) and the Arguments (Args vector)

*/

static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
    std::string IdName = IdentifierStr;

    getNextToken();

    if(CurTok != '('){
        return std::make_unique<VariableExprAST>(IdName);
        // This is a variable reference obviously.
    }
    getNextToken(); // Go to value after '('
    std::vector<std::unique_ptr<ExprAST>> Args;
    if(CurTok != ')'){
        while (true) {
            auto Arg = ParseExpression();
            if(Arg){
                Args.push_back(std::move(Arg));
            } else {
                return nullptr; // Improve upon this later
            }

            if (CurTok == ')') {
                break;
            }
            if(CurTok != ',') {
                return LogError("Expected ')' or ',' in argument list");
            }
            getNextToken();
        }
    }

    getNextToken();
    return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  default:
    return LogError("unknown token when expecting an expression");
  case tok_identifier:
    return ParseIdentifierExpr();
  case tok_number:
    return ParseNumberExpr();
  case '(':
    return ParseParenExpr();
  }
}

static std::map<char, int> BinopPrecendence = {{'<', 10}, {'+', 20}, 
                                               {'-', 20}, {'*', 40}, 
                                               {'/', 40}};

static int GetTokPrecendence() {
    if (!isascii(CurTok)) {
        return -1;
    }
    int TokPrec = BinopPrecendence[CurTok];
    if(TokPrec <= 0) {
        return -1;
    }
    return TokPrec;
}

static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS) {
    while(true) {
        int TokPrec = GetTokPrecendence();

        if(TokPrec < ExprPrec){
            return LHS;
        }

        int BinOp = CurTok;
        getNextToken();

        auto RHS = ParsePrimary();
        if(!RHS) {
            return nullptr;
        }

        int NextPrec = GetTokPrecendence();
        if(TokPrec < NextPrec){
            RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
            if(!RHS) {
                return nullptr;
            }
        }
        LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
    }
}

static std::unique_ptr<ExprAST> ParseExpression() {
    auto LHS = ParsePrimary();
    if(!LHS){
        return nullptr;
    }
    return ParseBinOpRHS(0, std::move(LHS));
}


static std::unique_ptr<PrototypeAST> ParsePrototype() {
  if (CurTok != tok_identifier)
    return LogErrorP("Expected function name in prototype");

  std::string FnName = IdentifierStr;
  getNextToken();

  if (CurTok != '(')
    return LogErrorP("Expected '(' in prototype");

  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(IdentifierStr);
  if (CurTok != ')')
    return LogErrorP("Expected ')' in prototype");

  // success.
  getNextToken(); // eat ')'.

  return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

static std::unique_ptr<FunctionAST> ParseDefinition() {
  getNextToken(); // eat def.
  auto Proto = ParsePrototype();
  if (!Proto)
    return nullptr;

  if (auto E = ParseExpression())
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  return nullptr;
}

static std::unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken();
    return ParsePrototype();
}

static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
    auto E = ParseExpression();
    if(E){
        auto Proto = std::make_unique<PrototypeAST>("", std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}

static void HandleDefinition() {
  if (ParseDefinition()) {
    fprintf(stderr, "Parsed a function definition.\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleExtern() {
  if (ParseExtern()) {
    fprintf(stderr, "Parsed an extern\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (ParseTopLevelExpr()) {
    fprintf(stderr, "Parsed a top-level expr\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void MainLoop() {
    while (true) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
        case tok_eof:
            return;
        case ';': // ignore top-level semicolons.
            getNextToken();
            break;
        case tok_def:
            HandleDefinition();
            break;
        case tok_extern:
            HandleExtern();
            break;
        default:
            HandleTopLevelExpression();
            break;
        }
    }

}

int main() {
    // Prime the first token.
    fprintf(stderr, "ready> ");
    
    getNextToken();

    MainLoop();

    return 0;
}