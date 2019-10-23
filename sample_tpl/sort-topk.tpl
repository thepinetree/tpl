struct State {
  sorter: Sorter
}

struct Row {
  a: Integer 
  b: Integer
}

fun compareFn(lhs: *Row, rhs: *Row) -> int32 {
  if (lhs.a < rhs.a) {
    return -1
  } else {
    return 1
  }
}

fun setUpState(execCtx: *ExecutionContext, state: *State) -> nil {
  @sorterInit(&state.sorter, @execCtxGetMem(execCtx), compareFn, @sizeOf(Row))
}

fun tearDownState(state: *State) -> nil {
  @sorterFree(&state.sorter)
}

fun pipeline_1(state: *State) -> nil {
  var sorter = &state.sorter
  var tvi: TableVectorIterator
  var top_k: uint32 = 11
  for (@tableIterInit(&tvi, "test_1"); @tableIterAdvance(&tvi); ) {
    var vpi = @tableIterGetVPI(&tvi)

    // Filter
    var filter: VectorFilterExecutor
    @filterExecInit(&filter, vpi)
    @filterExecLt(&filter, 0, @intToSql(2000))
    @filterExecFinish(&filter)
    @filterExecFree(&filter)

    // Insert into sorter
    for (; @vpiHasNextFiltered(vpi); @vpiAdvanceFiltered(vpi)) {
      var row = @ptrCast(*Row, @sorterInsertTopK(sorter, top_k))
      row.a = @vpiGetInt(vpi, 0)
      row.b = @vpiGetInt(vpi, 1)
      @sorterInsertTopKFinish(sorter, top_k)
    }
    @vpiResetFiltered(vpi)
  }
  @tableIterClose(&tvi)
}

fun pipeline_2(state: *State) -> int32 {
  var ret = 0
  var sort_iter: SorterIterator
  for (@sorterIterInit(&sort_iter, &state.sorter);
       @sorterIterHasNext(&sort_iter);
       @sorterIterNext(&sort_iter)) {
    var row = @ptrCast(*Row, @sorterIterGetRow(&sort_iter))
    ret = ret + 1
  }
  @sorterIterClose(&sort_iter)
  return ret
}

fun main(execCtx: *ExecutionContext) -> int32 {
  var state: State

  // Initialize
  setUpState(execCtx, &state)

  // Pipeline 1
  pipeline_1(&state)

  // Pipeline 1 end
  @sorterSort(&state.sorter)

  // Pipeline 2
  var ret = pipeline_2(&state)

  // Cleanup
  tearDownState(&state)

  return ret
}