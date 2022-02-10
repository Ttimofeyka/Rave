public import lexer;
public import logger;
public import tokens;
import std.conv : to;
import std.stdio;

int[] ints;
const char*[] strs;

extern(C) 
{
    int[] getIntsFromD() {
		return ints;
    }
	const char*[] getStrsFromD() {
		return strs;
	}
}

int getIntFromType(tok_type ttype) {
	switch(a) {
        case 0:
            return tok_type.tok_int;
        case 1:
            return tok_type.tok_char;
        case 2:
            return tok_type.tok_string;
        case 3:
            return tok_type.tok_hex;
        case 4:
            return tok_type.tok_void;
        case 5:
            return tok_type.tok_def;
        case 6:
            return tok_type.tok_ret;
        case 7:
            return tok_type.tok_intcmd;
        case 8:
            return tok_type.tok_charcmd;
        case 9:
            return tok_type.tok_stringcmd;
        case 10:
            return tok_type.tok_set;
        case 11:
            return tok_type.tok_if;
        case 12:
            return tok_type.tok_else;
        case 13:
            return tok_type.tok_while;
        case 14:
            return tok_type.tok_forr;
        case 15:
            return tok_type.tok_sarrcode;
        case 16:
            return tok_type.tok_earrcode;
        case 17:
            return tok_type.tok_sarr;
        case 18:
            return tok_type.tok_earr;
        case 19:
            return tok_type.tok_lparen;
        case 20:
            return tok_type.tok_rparen;
        case 21:
            return tok_type.tok_and;
        case 22:
            return tok_type.tok_nand;
        case 23:
            return tok_type.tok_equal;
        case 24:
            return tok_type.tok_nequal;
        case 25:
            return tok_type.tok_as;
        case 26:
            return tok_type.tok_zap;
        case 27:
            return tok_type.tok_plus;
        case 28:
            return tok_type.tok_minus;
        case 29:
            return tok_type.tok_multiply;
        case 30:
            return tok_type.tok_divide;
        case 31:
            return tok_type.tok_shortplu;
        case 32:
            return tok_type.tok_shortmin;
        case 33:
            return tok_type.tok_shortmul;
        case 34:
            return tok_type.tok_shortdiv;
        case 35:
            return tok_type.tok_identifier;
        case 36:
            return tok_type.tok_endl;
        case 37:
            return tok_type.tok_none;
        default: break;
    }
}

void main()
{
	Lexer lexer = new Lexer(
	"
	def main : void {
		ret 0;
	}
	");
	int[int] a;
	const char*[const char*] b;
	int currab = 0;
	for(int i=0; i<lexer.get_tokenlist().length; i++) {
		tok_type currtype = lexer.get_tokenlist().get_token(i).get_type();
		const char* currval = lexer.get_tokenlist().get_token(i).get_val().ptr;
		if(currtype == tok_type.tok_def
		||currtype == tok_type.tok_identifier) {
			a[currab] = getIntFromType(currtype);
			b[currab] = currval;
		}
		else {
			a[currab] = getIntFromType(currtype);
			a[currab] = "".ptr;
		}
		currab += 1;
	}

	ints = ints[a.length+1];
	strs = strs[strs.length+1];
}