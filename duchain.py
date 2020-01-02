import os, sys
from intervaltree import Interval, IntervalTree
import numpy as np
import collections

def merge_two_dicts(x, y):
    return {k: x.get(k, 0) + y.get(k, 0) for k in set(x) | set(y)}

if len(sys.argv) != 3:
    print("Usage: python duchain.py [program name] [tc id]")
else:
    program = sys.argv[1]
    ID = int(sys.argv[2])

    """ Update list of functions and function correlation matrix"""

    functions = []

    with open("%s_functions"%(program), "r") as f:
        for i, line in enumerate(f.readlines()):
            function = line[:-1]
            functions += [function]
    functions = list(set(functions))

    # DU-clear (include repeated calls)
    function_correlation_num = {}
    function_correlation_size = {}
    function_correlation_loop_call = {}
    function_correlation_call = {}
    function_correlation_execute = {}
    function_executed = {}
    temp_function_pairs = {}
    for i in functions:
        function_correlation_num[i] = {}
        function_correlation_size[i] = {}
        function_correlation_loop_call[i] = {}
        function_correlation_call[i] = {}
        function_correlation_execute[i] = {}
        function_executed[i] = 0
        temp_function_pairs[i] = {}
        for j in functions:
            function_correlation_num[i][j] = 0
            function_correlation_size[i][j] = 0
            function_correlation_loop_call[i][j] = []
            function_correlation_call[i][j] = 0
            function_correlation_execute[i][j] = 0
            temp_function_pairs[i][j] = 0

    """ Def-Use Chain"""
    function_execution_flag = {}
    function_call_flag = {}
    cur_line = 0
    callback_function = "???"
    DEF_list = IntervalTree()
    LOCAL_list = {}
    for func in functions:
        LOCAL_list[func] = IntervalTree()
    du_data = {}
    address_to_check_for_USE = {}
    call_stack = ['main']
    arg_stack = []
    nested_callexpr_stack = []
    for def_func in functions:
        for use_func in functions:
            if def_func != use_func:
                du_data[(def_func, use_func)] = {}

    with open("TRACE", "r") as f:
        for line in f:
            if len(line)>3 and line[:3] in ["DEF", "USE"]:
                tag = line[:3]
                fields = line[5:-1].split('\t')
                if len(fields) < 7: break
                var_name = fields[0]
                if fields[1] == "(nil)":
                    cur_line += 1
                    continue
                address = int(fields[1], 16)
                size = int(fields[2])
                func_name = fields[4]
                if func_name not in functions:
                    cur_line += 1
                    continue
                line_num = int(fields[5])
                top_level = int(fields[6])

                functions_to_remove = {}
                # callback functions
                if func_name in call_stack:
                    while call_stack[-1] != func_name:
                        if call_stack[-1] in functions:
                            for key, val2 in temp_function_pairs[call_stack[-1]].items():
                                if val2 > 0:
                                    function_correlation_loop_call[call_stack[-1]][key] += [val2]
                                    temp_function_pairs[call_stack[-1]][key] = 0
                        top = call_stack.pop()
                        functions_to_remove[top] = True

                target_mem_beg = address
                target_mem_end = address+size

                if (target_mem_beg, target_mem_end) in address_to_check_for_USE.keys():
                    for item in address_to_check_for_USE[(target_mem_beg, target_mem_end)].keys():
                        if len(item) == 2:
                            beg, end = item
                            for node in DEF_list[beg:end]:
                                def_beg, def_end = node.begin, node.end
                                def_func, def_var, def_line = node.data
                                # local variables
                                if def_func not in call_stack and len(LOCAL_list[def_func][def_beg:def_end]) != 0:
                                    DEF_list.discard(node)
                                    address_to_check_for_USE.pop((def_beg, def_end), None)
                                    for key in address_to_check_for_USE.keys():
                                        address_to_check_for_USE[key].pop((def_beg, def_end), None)
                                    functions_to_remove[def_func] = True
                                elif tag == "DEF" and (beg, end) == (target_mem_beg, target_mem_end):
                                    if def_beg == beg and end == def_end:
                                        DEF_list.discard(node)
                                        address_to_check_for_USE.pop((def_beg, def_end), None)
                                        for key in address_to_check_for_USE.keys():
                                            address_to_check_for_USE[key].pop((def_beg, def_end), None)
                                    elif def_beg <= beg and end <= def_end:
                                        continue
                                    elif def_beg >= beg and end >= def_end:
                                        DEF_list.discard(node)
                                        address_to_check_for_USE.pop((def_beg, def_end), None)
                                        for key in address_to_check_for_USE.keys():
                                            address_to_check_for_USE[key].pop((def_beg, def_end), None)
                                    elif def_beg < beg < def_end or def_beg < end < def_end:
                                        DEF_list.discard(node)
                                        address_to_check_for_USE.pop((def_beg, def_end), None)
                                        for key in address_to_check_for_USE.keys():
                                            address_to_check_for_USE[key].pop((def_beg, def_end), None)
                else:
                    for node in DEF_list[target_mem_beg:target_mem_end]:
                        def_beg, def_end = node.begin, node.end
                        def_func, def_var, def_line = node.data
                        if def_func not in call_stack and len(LOCAL_list[def_func][def_beg:def_end]) != 0:
                            DEF_list.discard(node)
                            address_to_check_for_USE.pop((def_beg, def_end), None)
                            for key in address_to_check_for_USE.keys():
                                address_to_check_for_USE[key].pop((def_beg, def_end), None)
                            functions_to_remove[def_func] = True
                        elif tag == "DEF":
                            if def_beg == target_mem_beg and target_mem_end == def_end:
                                DEF_list.discard(node)
                                address_to_check_for_USE.pop((def_beg, def_end), None)
                                for key in address_to_check_for_USE.keys():
                                    address_to_check_for_USE[key].pop((def_beg, def_end), None)
                            elif def_beg <= target_mem_beg and target_mem_end <= def_end:
                                continue
                            elif def_beg >= target_mem_beg and target_mem_end >= def_end:
                                DEF_list.discard(node)
                                address_to_check_for_USE.pop((def_beg, def_end), None)
                                for key in address_to_check_for_USE.keys():
                                    address_to_check_for_USE[key].pop((def_beg, def_end), None)
                            elif def_beg < target_mem_beg < def_end or def_beg < target_mem_end < def_end:
                                DEF_list.discard(node)
                                address_to_check_for_USE.pop((def_beg, def_end), None)
                                for key in address_to_check_for_USE.keys():
                                    address_to_check_for_USE[key].pop((def_beg, def_end), None)

                for f in functions_to_remove:
                    for interval_obj in LOCAL_list[f]:
                        iv_beg, iv_end = interval_obj.begin, interval_obj.end
                        address_to_check_for_USE.pop((iv_beg, iv_end), None)
                        for node in DEF_list[iv_beg:iv_end]:
                            DEF_list.discard(node)
                            address_to_check_for_USE.pop((node.begin, node.end), None)
                            for key in address_to_check_for_USE.keys():
                                address_to_check_for_USE[key].pop((node.begin, node.end), None)
                    LOCAL_list[f] = IntervalTree()

                if tag == "DEF":
                    # ASSIGN before DEF (e.g., int x = y;)
                    if (target_mem_beg, target_mem_end) not in address_to_check_for_USE:
                        address_to_check_for_USE[(target_mem_beg, target_mem_end)] = {(target_mem_beg, target_mem_end): 1}

                    if Interval(target_mem_beg, target_mem_end, True) not in LOCAL_list[func_name] and len(LOCAL_list[def_func][target_mem_beg:target_mem_end]) != 0:
                        LOCAL_list[func_name][target_mem_beg:target_mem_end] = True

                    DEF_list[target_mem_beg:target_mem_end] = (func_name, var_name, line_num)

                elif tag == "USE":
                    equal_flag = False

                    if (target_mem_beg, target_mem_end) in address_to_check_for_USE.keys():
                        memory_space_list = [(key, value) for key, value in address_to_check_for_USE[(target_mem_beg, target_mem_end)].iteritems()]
                    else:
                        memory_space_list = [((target_mem_beg, target_mem_end), 1)]

                    for item1, item2 in memory_space_list:
                        if len(item1) == 2:
                            beg, end = item1
                            for interval_obj in DEF_list[beg:end]:
                                def_func, def_var, def_line = interval_obj.data
                                if def_func != func_name:
                                    def_size = interval_obj.end - interval_obj.begin
                                    if (beg, end) not in du_data[(def_func, func_name)]:
                                        du_data[(def_func, func_name)][(beg, end)] = True
                                        function_correlation_num[def_func][func_name] += 1
                                        if top_level != 0:
                                            SIZE = top_level
                                        else:
                                            SIZE = min(end-beg,def_size)
                                        function_correlation_size[def_func][func_name] += SIZE
                        elif len(item1) == 3:
                            src_func, _, size = item1
                            if src_func == "CALLBACK" and callback_function != "???": 
                                src_func = callback_function

                            if src_func in functions and func_name in functions and src_func != func_name:
                                if (src_func, func_name, size) not in du_data[(src_func, func_name)]:
                                    du_data[(src_func, func_name)][(src_func, func_name, size)] = 1
                                    # for _ in range(item2):
                                    function_correlation_num[src_func][func_name] += 1
                                    function_correlation_size[src_func][func_name] += size

            elif len(line)>6 and line[:6] == "ASSIGN":
                fields = line[8:-1].split('\t')

                # assigning variable
                if len(fields) == 10:
                    lhs_name = fields[0]
                    if fields[1] == "(nil)":
                        cur_line += 1
                        continue
                    lhs_address = int(fields[1], 16)
                    lhs_size = int(fields[2])
                    rhs_name = fields[3]
                    if fields[4] == "(nil)":
                        cur_line += 1
                        continue
                    rhs_address = int(fields[4], 16)
                    rhs_size = int(fields[5])
                    func_name = fields[7]

                    lhs_beg = lhs_address
                    lhs_end = lhs_address + lhs_size
                    rhs_beg = rhs_address
                    rhs_end = rhs_address + rhs_size

                    functions_to_remove = {}
                    if (lhs_beg, lhs_end) in address_to_check_for_USE.keys():
                        for item in address_to_check_for_USE[(lhs_beg,lhs_end)].keys():
                            if len(item) == 2:
                                beg, end = item
                                for node in DEF_list[beg:end]:
                                    def_beg, def_end = node.begin, node.end
                                    def_func, def_var, def_line = node.data
                                    if def_func not in call_stack and len(LOCAL_list[def_func][def_beg:def_end]) != 0:
                                        DEF_list.discard(node)
                                        address_to_check_for_USE.pop((def_beg, def_end), None)
                                        for key in address_to_check_for_USE:
                                            address_to_check_for_USE[key].pop((def_beg, def_end), None)
                                        functions_to_remove[def_func] = True
                    else:
                        for node in DEF_list[lhs_beg:lhs_end]:
                            def_beg, def_end = node.begin, node.end
                            def_func, def_var, def_line = node.data
                            if def_func not in call_stack and len(LOCAL_list[def_func][def_beg:def_end]) != 0:
                                DEF_list.discard(node)
                                address_to_check_for_USE.pop((def_beg, def_end), None)
                                for key in address_to_check_for_USE:
                                    address_to_check_for_USE[key].pop((def_beg, def_end), None)
                                functions_to_remove[def_func] = True

                    for f in functions_to_remove:
                        for interval_obj in LOCAL_list[f]:
                            iv_beg, iv_end = interval_obj.begin, interval_obj.end
                            address_to_check_for_USE.pop((iv_beg, iv_end), None)
                            for node in DEF_list[iv_beg:iv_end]:
                                DEF_list.discard(node)
                                address_to_check_for_USE.pop((node.begin, node.end), None)
                                for key in address_to_check_for_USE.keys():
                                    address_to_check_for_USE[key].pop((node.begin, node.end), None)
                        LOCAL_list[f] = IntervalTree()

                    # DEF is before ASSIGN in the same function
                    if (lhs_beg, lhs_end) not in address_to_check_for_USE:
                        address_to_check_for_USE[(lhs_beg, lhs_end)] = {(lhs_beg, lhs_end): 1}

                    if (rhs_beg, rhs_end) not in address_to_check_for_USE:
                        address_to_check_for_USE[(rhs_beg, rhs_end)] = {(rhs_beg, rhs_end): 1}
                    address_to_check_for_USE[(lhs_beg, lhs_end)] = merge_two_dicts(address_to_check_for_USE[(lhs_beg, lhs_end)], address_to_check_for_USE[(rhs_beg, rhs_end)])
                    

                # assigning return value of function
                elif len(fields) == 8:
                    lhs_name = fields[0]
                    if fields[1] == "(nil)":
                        cur_line += 1
                        continue
                    lhs_address = int(fields[1], 16)
                    lhs_size = int(fields[2])
                    func_name = fields[3]
                    return_size = int(fields[4])
                    caller_name = fields[6]

                    lhs_beg = lhs_address
                    lhs_end = lhs_address+lhs_size

                    functions_to_remove = {}
                    if (lhs_beg, lhs_end) in address_to_check_for_USE.keys():
                        for item in address_to_check_for_USE[(lhs_beg,lhs_end)].keys():
                            if len(item) == 2:
                                beg, end = item
                                for node in DEF_list[beg:end]:
                                    def_beg, def_end = node.begin, node.end
                                    def_func, def_var, def_line = node.data
                                    if def_func not in call_stack and len(LOCAL_list[def_func][def_beg:def_end]) != 0:
                                        DEF_list.discard(node)
                                        address_to_check_for_USE.pop((def_beg, def_end), None)
                                        for key in address_to_check_for_USE:
                                            address_to_check_for_USE[key].pop((def_beg, def_end), None)
                                        functions_to_remove[def_func] = True

                    else:
                        for node in DEF_list[lhs_beg:lhs_end]:
                            def_beg, def_end = node.begin, node.end
                            def_func, def_var, def_line = node.data
                            if def_func not in call_stack and len(LOCAL_list[def_func][def_beg:def_end]) != 0:
                                DEF_list.discard(node)
                                address_to_check_for_USE.pop((def_beg, def_end), None)
                                for key in address_to_check_for_USE:
                                    address_to_check_for_USE[key].pop((def_beg, def_end), None)
                                functions_to_remove[def_func] = True

                    for f in functions_to_remove:
                        for interval_obj in LOCAL_list[f]:
                            iv_beg, iv_end = interval_obj.begin, interval_obj.end
                            address_to_check_for_USE.pop((iv_beg, iv_end), None)
                            for node in DEF_list[iv_beg:iv_end]:
                                DEF_list.discard(node)
                                address_to_check_for_USE.pop((node.begin, node.end), None)
                                for key in address_to_check_for_USE.keys():
                                    address_to_check_for_USE[key].pop((node.begin, node.end), None)
                        LOCAL_list[f] = IntervalTree()

                    # DEF is before ASSIGN in the same function
                    if (lhs_beg, lhs_end) not in address_to_check_for_USE:
                        address_to_check_for_USE[(lhs_beg, lhs_end)] = {(lhs_beg, lhs_end): 1}
                    if (func_name, caller_name, return_size) not in address_to_check_for_USE[(lhs_beg, lhs_end)]:
                        address_to_check_for_USE[(lhs_beg, lhs_end)][(func_name, caller_name, return_size)] = 1
                    else:
                        address_to_check_for_USE[(lhs_beg, lhs_end)][(func_name, caller_name, return_size)] += 1
            
                else:
                    cur_line += 1
                    break

            elif len(line)>12 and line[:12] == "RETURN VALUE":
                fields = line[14:-1].split('\t')
                if len(fields) != 3:
                    cur_line += 1
                    continue
                f_name = fields[0]
                g_name = fields[1]
                if f_name not in functions or g_name not in functions or f_name == g_name:
                    cur_line += 1
                    continue
                data_flow_size = int(fields[2])

                if (f_name, g_name, data_flow_size) not in du_data[(f_name, g_name)]:
                    du_data[(f_name, g_name)][(f_name, g_name, data_flow_size)] = True
                    function_correlation_num[f_name][g_name] += 1
                    function_correlation_size[f_name][g_name] += data_flow_size

            elif len(line)>4 and line[:4] == "CALL":
                fields = line[6:-1].split('\t')
                if len(fields) != 3:
                    cur_line += 1
                    continue
                caller_name = fields[0]
                callee_name = fields[1]
                if callee_name not in functions:
                    cur_line += 1
                    continue
                num_arg = int(fields[2])
                if num_arg > 0:
                    arg_stack.append({})
                    nested_callexpr_stack.append((caller_name, callee_name, num_arg))

                # denom
                if caller_name in functions: function_execution_flag[caller_name] = True
                if callee_name in functions: function_execution_flag[callee_name] = True

                functions_to_remove = {}

                #print call_stack, caller_name, callee_name
                if caller_name in call_stack:
                    while call_stack[-1] != caller_name:
                        if call_stack[-1] in functions:
                            for key, val2 in temp_function_pairs[call_stack[-1]].items():
                                if val2 > 0:
                                    function_correlation_loop_call[call_stack[-1]][key] += [val2]
                                    temp_function_pairs[call_stack[-1]][key] = 0
                        top = call_stack.pop()
                        functions_to_remove[top] = True

                if callee_name in functions:
                    call_stack.append(callee_name)
                    temp_function_pairs[caller_name][callee_name] += 1
                    for x in range(0, len(call_stack)-1):
                        if call_stack[x] not in function_call_flag:
                            function_call_flag[call_stack[x]] = {}
                        function_call_flag[call_stack[x]][callee_name] = True

                for f in functions_to_remove:
                    for interval_obj in LOCAL_list[f]:
                        iv_beg, iv_end = interval_obj.begin, interval_obj.end
                        address_to_check_for_USE.pop((iv_beg, iv_end), None)
                        for node in DEF_list[iv_beg:iv_end]:
                            DEF_list.discard(node)
                            address_to_check_for_USE.pop((node.begin, node.end), None)
                            for key in address_to_check_for_USE.keys():
                                address_to_check_for_USE[key].pop((node.begin, node.end), None)
                    LOCAL_list[f] = IntervalTree()

            elif len(line)>5 and line[:5] == "LOCAL":
                fields = line[7:-1].split('\t')
                if len(fields) != 7:
                    cur_line += 1
                    continue
                if fields[1] == "(nil)":
                    cur_line += 1
                    continue
                var_name = fields[0]
                address = int(fields[1], 16)
                size = int(fields[2])
                func_name = fields[4]

                beg = address
                end = address+size
                LOCAL_list[func_name][beg:end] = True

            elif len(line)>3 and line[:3] == "ARG":
                fields = line[5:-1].split('\t')

                if len(fields) == 3:
                    arg_id = int(fields[0])
                    f_name = fields[1]
                    g_name = fields[2]

                    if g_name == "CALLBACK" and len(arg_stack) == 0:
                        arg_stack.append({})
                        nested_callexpr_stack.append((f_name, g_name, arg_id))
                    
                    if g_name in functions or g_name == "CALLBACK":
                        assert(len(arg_stack) > 0)
                        if g_name in functions:
                            if call_stack[-1] != nested_callexpr_stack[-1][1]:
                                functions_to_remove = {}
                                while call_stack[-1] != nested_callexpr_stack[-1][0]:
                                    top = call_stack.pop()
                                    functions_to_remove[top] = True
                                call_stack.append(nested_callexpr_stack[-1][1])

                                for x in range(0, len(call_stack)-1):
                                    if call_stack[x] not in function_call_flag:
                                        function_call_flag[call_stack[x]] = {}
                                    function_call_flag[call_stack[x]][nested_callexpr_stack[-1][1]] = True

                                for f in functions_to_remove:
                                    for interval_obj in LOCAL_list[f]:
                                        iv_beg, iv_end = interval_obj.begin, interval_obj.end
                                        address_to_check_for_USE.pop((iv_beg, iv_end), None)
                                        for node in DEF_list[iv_beg:iv_end]:
                                            DEF_list.discard(node)
                                            address_to_check_for_USE.pop((node.begin, node.end), None)
                                            for key in address_to_check_for_USE.keys():
                                                address_to_check_for_USE[key].pop((node.begin, node.end), None)
                                    LOCAL_list[f] = IntervalTree()

                        arg_stack[-1][arg_id] = (f_name, g_name)

                elif len(fields) == 7:
                    arg_id = int(fields[1])
                    f_name = fields[5]
                    g_name = fields[0]
                    var_name = fields[2]
                    address = int(fields[3], 16)

                    if g_name == "CALLBACK" and len(arg_stack) == 0:
                        arg_stack.append({})
                        nested_callexpr_stack.append((f_name, g_name, arg_id))

                    if g_name in functions or g_name == "CALLBACK":
                        assert(len(arg_stack) > 0)

                        if g_name in functions:
                            if call_stack[-1] != nested_callexpr_stack[-1][1]:
                                functions_to_remove = {}
                                while call_stack[-1] != nested_callexpr_stack[-1][0]:
                                    top = call_stack.pop()
                                    functions_to_remove[top] = True
                                call_stack.append(nested_callexpr_stack[-1][1])

                                for x in range(0, len(call_stack)-1):
                                    if call_stack[x] not in function_call_flag:
                                        function_call_flag[call_stack[x]] = {}
                                    function_call_flag[call_stack[x]][nested_callexpr_stack[-1][1]] = True

                                for f in functions_to_remove:
                                    for interval_obj in LOCAL_list[f]:
                                        iv_beg, iv_end = interval_obj.begin, interval_obj.end
                                        address_to_check_for_USE.pop((iv_beg, iv_end), None)
                                        for node in DEF_list[iv_beg:iv_end]:
                                            DEF_list.discard(node)
                                            address_to_check_for_USE.pop((node.begin, node.end), None)
                                            for key in address_to_check_for_USE.keys():
                                                address_to_check_for_USE[key].pop((node.begin, node.end), None)
                                    LOCAL_list[f] = IntervalTree()

                        arg_stack[-1][arg_id] = (f_name, var_name, address)

                else: 
                    cur_line += 1
                    continue

            elif len(line)>5 and line[:5] == "PARAM":
                fields = line[7:-1].split('\t')
                if len(fields) != 8:
                    cur_line += 1
                    continue
                if fields[1] == "(nil)":
                    cur_line += 1
                    continue
                param_id = int(fields[0])
                var_name = fields[1]
                address = int(fields[2], 16)
                size = int(fields[3])
                func_name = fields[5]

                beg = address
                end = address+size
                LOCAL_list[func_name][beg:end] = True

                if (beg, end) not in address_to_check_for_USE:
                    address_to_check_for_USE[(beg, end)] = {(beg, end): 1}

                if len(arg_stack) > 0:
                    assert(param_id in arg_stack[-1])
                    if len(arg_stack[-1][param_id]) == 2:
                        f_name, g_name = arg_stack[-1][param_id]
                        if g_name == 'CALLBACK':
                            if (f_name, func_name, size) not in address_to_check_for_USE[(beg, end)]:
                                address_to_check_for_USE[(beg, end)][(f_name, func_name, size)] = 1
                            else:
                                address_to_check_for_USE[(beg, end)][(f_name, func_name, size)] += 1
                        else:
                            if (f_name, g_name, size) not in address_to_check_for_USE[(beg, end)]:
                                address_to_check_for_USE[(beg, end)][(f_name, g_name, size)] = 1
                            else:
                                address_to_check_for_USE[(beg, end)][(f_name, g_name, size)] += 1
                    else:
                        _, _, arg_address = arg_stack[-1][param_id]
                        arg_beg = arg_address
                        arg_end = arg_address+size

                        if (arg_beg, arg_end) not in address_to_check_for_USE:
                            address_to_check_for_USE[(arg_beg, arg_end)] = {(arg_beg, arg_end): 1}
                        address_to_check_for_USE[(beg, end)] = merge_two_dicts(address_to_check_for_USE[(beg, end)], address_to_check_for_USE[(arg_beg, arg_end)])

                    if param_id == 1:
                        arg_stack.pop()
                        nested_callexpr_stack.pop()

            cur_line += 1

        for func in function_execution_flag:
            function_executed[func] += 1
        for func1 in function_execution_flag.keys():
            for func2 in function_execution_flag.keys():
                if func1 != func2:
                    function_correlation_execute[func1][func2] += 1
        for caller in function_call_flag:
            for callee in function_call_flag[caller]:
                function_correlation_call[caller][callee] += 1

    while call_stack:
        if call_stack[-1] in functions:
            for key, val2 in temp_function_pairs[call_stack[-1]].items():
                if val2 > 0:
                    function_correlation_loop_call[call_stack[-1]][key] += [val2]
                    temp_function_pairs[call_stack[-1]][key] = 0
        call_stack.pop()


    f = open("./function_correlation/%s_function_correlation_TC%d.csv"%(program, ID), "w")
    f.write("f in P(g|f),g in P(g|f),|f|,|f calls g|,|f and g are executed|,size,num,# caller being called\n")
    for func1 in functions:
        for func2 in functions:
            if func1 != func2:
                col3 = function_executed[func1]
                col4 = function_correlation_call[func1][func2]
                col5 = function_correlation_execute[func1][func2]
                col6 = function_correlation_size[func1][func2]
                col7 = function_correlation_num[func1][func2]
                col8 = sum(function_correlation_loop_call[func1][func2])
                f.write("%s,%s,%d,%d,%d,%d,%d,%d\n" %(func1,func2,col3,col4,col5,col6,col7,col8))
    f.close()
