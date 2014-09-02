import sys
from genutil import *

# Templates

reservedOperatorCaseTemplate = """
case operator_${{NAME}}
	expect compile_fail
	values {}

	both ""
		precision mediump float;
		precision mediump int;

		${DECLARATIONS}

		void main()
		{
			${SETUP}
			int value = 100;
			${{OP}}
			${OUTPUT}
		}
	""
end
"""[1:-1]

# Classes

class ReservedOperatorCase(ShaderCase):
	def __init__(self, op):
		self.name 		= op.name
		if op.operator == "~":
			self.operation = 'value = ~value;'
		else:
			self.operation 	= 'value ' + op.operator + ' 1;'

	def __str__(self):
		params = {
			"NAME": self.name,
			"OP"  : self.operation
		}
		return fillTemplate(reservedOperatorCaseTemplate, params)


class Operator():
	def __init__(self, operator, name):
		self.operator 	= operator
		self.name 		= name

# Declarations

RESERVED_OPERATORS = [
	Operator("%",	"modulo"),
	Operator("~",	"bitwise_not"),
	Operator("<<",	"bitwise_shift_left"),
	Operator(">>",	"bitwise_shift_right"),
	Operator("&",	"bitwise_and"),
	Operator("^",	"bitwise_xor"),
	Operator("|",	"bitwise_or"),
	Operator("%=",	"assign_modulo"),
	Operator("<<=",	"assign_shift_left"),
	Operator(">>=", "assign_shift_right"),
	Operator("&=",	"assign_and"),
	Operator("^=",	"assign_xor"),
	Operator("|=",	"assign_or")
]

# Reserved operator usage cases

reservedOperatorCases = []

for operator in RESERVED_OPERATORS:
	reservedOperatorCases.append(ReservedOperatorCase(operator)) 		# Reserved operators

# Main program

if __name__ == "__main__":
	print "Generating shader case files."
	writeAllCases("reserved_operators.test", 	reservedOperatorCases)