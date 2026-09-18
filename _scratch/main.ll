; ModuleID = 'main.sere'
source_filename = "main.sere"
target triple = "x86_64-pc-windows-msvc"

%Exception = type { i32, { ptr, i64 } }

@sere.module.init.done = internal global i8 0
@0 = private unnamed_addr constant [10 x i8] c"Exception\00", align 1
@1 = private unnamed_addr constant [8 x i8] c"Jessica\00", align 1
@2 = private unnamed_addr constant [21 x i8] c"Cannot greet Jessica\00", align 1
@3 = private unnamed_addr constant [8 x i8] c"Hello, \00", align 1
@4 = private unnamed_addr constant [2 x i8] c"!\00", align 1
@5 = private unnamed_addr constant [5 x i8] c"John\00", align 1
@6 = private unnamed_addr constant [2 x i8] c" \00", align 1
@7 = private unnamed_addr constant [8 x i8] c"Jessica\00", align 1
@8 = private unnamed_addr constant [2 x i8] c" \00", align 1
@9 = private unnamed_addr constant [5 x i8] c"None\00", align 1
@10 = private unnamed_addr constant [11 x i8] c"None | str\00", align 1
@11 = private unnamed_addr constant [10 x i8] c"Exception\00", align 1

declare void @sere_input(ptr, i64, ptr, ptr)

define i32 @abs(i32 %value) {
entry:
  %value1 = alloca i32, align 4
  store i32 %value, ptr %value1, align 4
  %0 = load i32, ptr %value1, align 4
  %1 = icmp slt i32 %0, 0
  br i1 %1, label %if.then, label %if.next

if.end:                                           ; preds = %if.next
  %2 = load i32, ptr %value1, align 4
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err2, label %err.ok3

if.then:                                          ; preds = %entry
  %5 = load i32, ptr %value1, align 4
  %6 = sub i32 0, %5
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

if.next:                                          ; preds = %entry
  br label %if.end

err:                                              ; preds = %if.then
  ret i32 0

err.ok:                                           ; preds = %if.then
  ret i32 %6

err2:                                             ; preds = %if.end
  ret i32 0

err.ok3:                                          ; preds = %if.end
  ret i32 %2
}

define i32 @min(i32 %left, i32 %right) {
entry:
  %left1 = alloca i32, align 4
  store i32 %left, ptr %left1, align 4
  %right2 = alloca i32, align 4
  store i32 %right, ptr %right2, align 4
  %0 = load i32, ptr %left1, align 4
  %1 = load i32, ptr %right2, align 4
  %2 = icmp slt i32 %0, %1
  br i1 %2, label %if.then, label %if.next

if.then:                                          ; preds = %entry
  %3 = load i32, ptr %left1, align 4
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

if.next:                                          ; preds = %entry
  %6 = load i32, ptr %right2, align 4
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err3, label %err.ok4

err:                                              ; preds = %if.then
  ret i32 0

err.ok:                                           ; preds = %if.then
  ret i32 %3

err3:                                             ; preds = %if.next
  ret i32 0

err.ok4:                                          ; preds = %if.next
  ret i32 %6
}

define i32 @max(i32 %left, i32 %right) {
entry:
  %left1 = alloca i32, align 4
  store i32 %left, ptr %left1, align 4
  %right2 = alloca i32, align 4
  store i32 %right, ptr %right2, align 4
  %0 = load i32, ptr %left1, align 4
  %1 = load i32, ptr %right2, align 4
  %2 = icmp sgt i32 %0, %1
  br i1 %2, label %if.then, label %if.next

if.then:                                          ; preds = %entry
  %3 = load i32, ptr %left1, align 4
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

if.next:                                          ; preds = %entry
  %6 = load i32, ptr %right2, align 4
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err3, label %err.ok4

err:                                              ; preds = %if.then
  ret i32 0

err.ok:                                           ; preds = %if.then
  ret i32 %3

err3:                                             ; preds = %if.next
  ret i32 0

err.ok4:                                          ; preds = %if.next
  ret i32 %6
}

define i32 @clamp(i32 %value, i32 %low, i32 %high) {
entry:
  %value1 = alloca i32, align 4
  store i32 %value, ptr %value1, align 4
  %low2 = alloca i32, align 4
  store i32 %low, ptr %low2, align 4
  %high3 = alloca i32, align 4
  store i32 %high, ptr %high3, align 4
  %0 = load i32, ptr %value1, align 4
  %1 = load i32, ptr %low2, align 4
  %2 = call i32 @max(i32 %0, i32 %1)
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i32 0

err.ok:                                           ; preds = %entry
  %5 = load i32, ptr %high3, align 4
  %6 = call i32 @min(i32 %2, i32 %5)
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err4, label %err.ok5

err4:                                             ; preds = %err.ok
  ret i32 0

err.ok5:                                          ; preds = %err.ok
  %9 = call i32 @sere_has_error()
  %10 = icmp ne i32 %9, 0
  br i1 %10, label %err6, label %err.ok7

err6:                                             ; preds = %err.ok5
  ret i32 0

err.ok7:                                          ; preds = %err.ok5
  ret i32 %6
}

define i32 @sign(i32 %value) {
entry:
  %value1 = alloca i32, align 4
  store i32 %value, ptr %value1, align 4
  %0 = load i32, ptr %value1, align 4
  %1 = icmp sgt i32 %0, 0
  br i1 %1, label %if.then, label %if.next

if.end:                                           ; preds = %if.next3
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err6, label %err.ok7

if.then:                                          ; preds = %entry
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

if.next:                                          ; preds = %entry
  %6 = load i32, ptr %value1, align 4
  %7 = icmp slt i32 %6, 0
  br i1 %7, label %if.then2, label %if.next3

err:                                              ; preds = %if.then
  ret i32 0

err.ok:                                           ; preds = %if.then
  ret i32 1

if.then2:                                         ; preds = %if.next
  %8 = call i32 @sere_has_error()
  %9 = icmp ne i32 %8, 0
  br i1 %9, label %err4, label %err.ok5

if.next3:                                         ; preds = %if.next
  br label %if.end

err4:                                             ; preds = %if.then2
  ret i32 0

err.ok5:                                          ; preds = %if.then2
  ret i32 -1

err6:                                             ; preds = %if.end
  ret i32 0

err.ok7:                                          ; preds = %if.end
  ret i32 0
}

define void @prelude_Exception___init__(ptr %self, { ptr, i64 } %message) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %message1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %message, ptr %message1, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Exception, ptr %0, i32 0, i32 1
  %2 = load { ptr, i64 }, ptr %message1, align 8
  store { ptr, i64 } %2, ptr %1, align 8
  ret void
}

define i32 @Result_unwrap(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load { i32, ptr }, ptr %self.slot, align 8
  %1 = extractvalue { i32, ptr } %0, 0
  %2 = icmp eq i32 %1, 0
  br i1 %2, label %match.arm, label %match.next

match.end:                                        ; preds = %match.next2, %err.ok8
  ret i32 0

match.arm:                                        ; preds = %entry
  %3 = extractvalue { i32, ptr } %0, 1
  %value = alloca i32, align 4
  %4 = getelementptr inbounds nuw { i32 }, ptr %3, i32 0, i32 0
  %5 = load i32, ptr %4, align 4
  store i32 %5, ptr %value, align 4
  %6 = load i32, ptr %value, align 4
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

match.next:                                       ; preds = %entry
  %9 = icmp eq i32 %1, 1
  br i1 %9, label %match.arm1, label %match.next2

err:                                              ; preds = %match.arm
  ret i32 0

err.ok:                                           ; preds = %match.arm
  ret i32 %6

match.arm1:                                       ; preds = %match.next
  %10 = extractvalue { i32, ptr } %0, 1
  %message = alloca { ptr, i64 }, align 8
  %11 = getelementptr inbounds nuw { { ptr, i64 } }, ptr %10, i32 0, i32 0
  %12 = load { ptr, i64 }, ptr %11, align 8
  store { ptr, i64 } %12, ptr %message, align 8
  %init.tmp = alloca %Exception, align 8
  store %Exception zeroinitializer, ptr %init.tmp, align 8
  %13 = getelementptr inbounds nuw %Exception, ptr %init.tmp, i32 0, i32 0
  store i32 -1845790122, ptr %13, align 4
  %14 = load { ptr, i64 }, ptr %message, align 8
  call void @prelude_Exception___init__(ptr %init.tmp, { ptr, i64 } %14)
  %15 = load %Exception, ptr %init.tmp, align 8
  %16 = call i32 @sere_has_error()
  %17 = icmp ne i32 %16, 0
  br i1 %17, label %err3, label %err.ok4

match.next2:                                      ; preds = %match.next
  br label %match.end

err3:                                             ; preds = %match.arm1
  ret i32 0

err.ok4:                                          ; preds = %match.arm1
  %18 = extractvalue %Exception %15, 1
  %19 = extractvalue { ptr, i64 } %18, 0
  %20 = extractvalue { ptr, i64 } %18, 1
  call void @sere_raise(ptr @0, ptr %19, i64 %20)
  %21 = alloca %Exception, align 8
  store %Exception %15, ptr %21, align 8
  call void @sere_error_set_object(ptr %21, i64 24)
  %22 = call i32 @sere_has_error()
  %23 = icmp ne i32 %22, 0
  br i1 %23, label %err5, label %err.ok6

err5:                                             ; preds = %err.ok4
  ret i32 0

err.ok6:                                          ; preds = %err.ok4
  %24 = call i32 @sere_has_error()
  %25 = icmp ne i32 %24, 0
  br i1 %25, label %err7, label %err.ok8

err7:                                             ; preds = %err.ok6
  ret i32 0

err.ok8:                                          ; preds = %err.ok6
  br label %match.end
}

define { i32, i64 } @Result_try_unwrap(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load { i32, ptr }, ptr %self.slot, align 8
  %1 = extractvalue { i32, ptr } %0, 0
  %2 = icmp eq i32 %1, 0
  br i1 %2, label %match.arm, label %match.next

match.end:                                        ; preds = %match.next2
  ret { i32, i64 } zeroinitializer

match.arm:                                        ; preds = %entry
  %3 = extractvalue { i32, ptr } %0, 1
  %value = alloca i32, align 4
  %4 = getelementptr inbounds nuw { i32 }, ptr %3, i32 0, i32 0
  %5 = load i32, ptr %4, align 4
  store i32 %5, ptr %value, align 4
  %6 = load i32, ptr %value, align 4
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

match.next:                                       ; preds = %entry
  %9 = icmp eq i32 %1, 1
  br i1 %9, label %match.arm1, label %match.next2

err:                                              ; preds = %match.arm
  ret { i32, i64 } zeroinitializer

err.ok:                                           ; preds = %match.arm
  ret { i32, i64 } { i32 1, i64 0 }

match.arm1:                                       ; preds = %match.next
  %10 = extractvalue { i32, ptr } %0, 1
  %11 = call i32 @sere_has_error()
  %12 = icmp ne i32 %11, 0
  br i1 %12, label %err3, label %err.ok4

match.next2:                                      ; preds = %match.next
  br label %match.end

err3:                                             ; preds = %match.arm1
  ret { i32, i64 } zeroinitializer

err.ok4:                                          ; preds = %match.arm1
  ret { i32, i64 } zeroinitializer
}

define i1 @Result_is_ok(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load { i32, ptr }, ptr %self.slot, align 8
  %1 = extractvalue { i32, ptr } %0, 0
  %2 = icmp eq i32 %1, 0
  br i1 %2, label %match.arm, label %match.next

match.end:                                        ; preds = %match.next2
  ret i1 false

match.arm:                                        ; preds = %entry
  %3 = extractvalue { i32, ptr } %0, 1
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

match.next:                                       ; preds = %entry
  %6 = icmp eq i32 %1, 1
  br i1 %6, label %match.arm1, label %match.next2

err:                                              ; preds = %match.arm
  ret i1 false

err.ok:                                           ; preds = %match.arm
  ret i1 true

match.arm1:                                       ; preds = %match.next
  %7 = extractvalue { i32, ptr } %0, 1
  %8 = call i32 @sere_has_error()
  %9 = icmp ne i32 %8, 0
  br i1 %9, label %err3, label %err.ok4

match.next2:                                      ; preds = %match.next
  br label %match.end

err3:                                             ; preds = %match.arm1
  ret i1 false

err.ok4:                                          ; preds = %match.arm1
  ret i1 false
}

define { i32, ptr } @greet({ ptr, i64 } %name) {
entry:
  %name1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %name, ptr %name1, align 8
  %0 = load { ptr, i64 }, ptr %name1, align 8
  %1 = extractvalue { ptr, i64 } %0, 0
  %2 = extractvalue { ptr, i64 } %0, 1
  %3 = call i32 @sere_str_cmp(ptr %1, i64 %2, ptr @1, i64 7)
  %4 = icmp eq i32 %3, 0
  br i1 %4, label %if.then, label %if.next

if.end:                                           ; preds = %if.next
  %5 = call ptr @sere_alloc(i64 16)
  %6 = load { ptr, i64 }, ptr %name1, align 8
  %cat.len = alloca i64, align 8
  %7 = extractvalue { ptr, i64 } %6, 0
  %8 = extractvalue { ptr, i64 } %6, 1
  %9 = call ptr @sere_str_concat_data(ptr @3, i64 7, ptr %7, i64 %8, ptr %cat.len)
  %10 = load i64, ptr %cat.len, align 4
  %11 = insertvalue { ptr, i64 } undef, ptr %9, 0
  %12 = insertvalue { ptr, i64 } %11, i64 %10, 1
  %cat.len4 = alloca i64, align 8
  %13 = extractvalue { ptr, i64 } %12, 0
  %14 = extractvalue { ptr, i64 } %12, 1
  %15 = call ptr @sere_str_concat_data(ptr %13, i64 %14, ptr @4, i64 1, ptr %cat.len4)
  %16 = load i64, ptr %cat.len4, align 4
  %17 = insertvalue { ptr, i64 } undef, ptr %15, 0
  %18 = insertvalue { ptr, i64 } %17, i64 %16, 1
  %19 = getelementptr inbounds nuw { { ptr, i64 } }, ptr %5, i32 0, i32 0
  store { ptr, i64 } %18, ptr %19, align 8
  %20 = insertvalue { i32, ptr } { i32 0, ptr undef }, ptr %5, 1
  %21 = call i32 @sere_has_error()
  %22 = icmp ne i32 %21, 0
  br i1 %22, label %err5, label %err.ok6

if.then:                                          ; preds = %entry
  %23 = call ptr @sere_alloc(i64 16)
  %24 = getelementptr inbounds nuw { { ptr, i64 } }, ptr %23, i32 0, i32 0
  store { ptr, i64 } { ptr @2, i64 20 }, ptr %24, align 8
  %25 = insertvalue { i32, ptr } { i32 1, ptr undef }, ptr %23, 1
  %26 = call i32 @sere_has_error()
  %27 = icmp ne i32 %26, 0
  br i1 %27, label %err, label %err.ok

if.next:                                          ; preds = %entry
  br label %if.end

err:                                              ; preds = %if.then
  ret { i32, ptr } zeroinitializer

err.ok:                                           ; preds = %if.then
  %28 = call i32 @sere_has_error()
  %29 = icmp ne i32 %28, 0
  br i1 %29, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret { i32, ptr } zeroinitializer

err.ok3:                                          ; preds = %err.ok
  ret { i32, ptr } %25

err5:                                             ; preds = %if.end
  ret { i32, ptr } zeroinitializer

err.ok6:                                          ; preds = %if.end
  %30 = call i32 @sere_has_error()
  %31 = icmp ne i32 %30, 0
  br i1 %31, label %err7, label %err.ok8

err7:                                             ; preds = %err.ok6
  ret { i32, ptr } zeroinitializer

err.ok8:                                          ; preds = %err.ok6
  ret { i32, ptr } %20
}

define i32 @sere_main() {
entry:
  %greet2_str = alloca { i32, i64 }, align 8
  %greet2 = alloca { i32, ptr }, align 8
  %greet1_str = alloca { ptr, i64 }, align 8
  %greet1 = alloca { i32, ptr }, align 8
  call void @sere.module.init()
  %0 = call { i32, ptr } @greet({ ptr, i64 } { ptr @5, i64 4 })
  %1 = call i32 @sere_has_error()
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i32 0

err.ok:                                           ; preds = %entry
  store { i32, ptr } %0, ptr %greet1, align 8
  %3 = call { ptr, i64 } @Result_str_unwrap(ptr %greet1)
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err1, label %err.ok2

err1:                                             ; preds = %err.ok
  ret i32 0

err.ok2:                                          ; preds = %err.ok
  store { ptr, i64 } %3, ptr %greet1_str, align 8
  %6 = load { ptr, i64 }, ptr %greet1_str, align 8
  %7 = extractvalue { ptr, i64 } %6, 0
  %8 = extractvalue { ptr, i64 } %6, 1
  call void @sere_write(ptr %7, i64 %8)
  call void @sere_write_nl()
  %9 = call i32 @sere_has_error()
  %10 = icmp ne i32 %9, 0
  br i1 %10, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok2
  ret i32 0

err.ok4:                                          ; preds = %err.ok2
  %11 = call { i32, ptr } @greet({ ptr, i64 } { ptr @7, i64 7 })
  %12 = call i32 @sere_has_error()
  %13 = icmp ne i32 %12, 0
  br i1 %13, label %err5, label %err.ok6

err5:                                             ; preds = %err.ok4
  ret i32 0

err.ok6:                                          ; preds = %err.ok4
  store { i32, ptr } %11, ptr %greet2, align 8
  %14 = call { i32, i64 } @Result_str_try_unwrap(ptr %greet2)
  %15 = call i32 @sere_has_error()
  %16 = icmp ne i32 %15, 0
  br i1 %16, label %err7, label %err.ok8

err7:                                             ; preds = %err.ok6
  ret i32 0

err.ok8:                                          ; preds = %err.ok6
  store { i32, i64 } %14, ptr %greet2_str, align 4
  %17 = load { i32, i64 }, ptr %greet2_str, align 4
  %18 = extractvalue { i32, i64 } %17, 0
  %19 = extractvalue { i32, i64 } %17, 1
  %union.str = alloca { ptr, i64 }, align 8
  switch i32 %18, label %union.str.def [
    i32 0, label %union.str.case
    i32 1, label %union.str.case9
  ]

union.str.end:                                    ; preds = %union.str.def, %union.str.case9, %union.str.case
  %20 = load { ptr, i64 }, ptr %union.str, align 8
  %21 = extractvalue { ptr, i64 } %20, 0
  %22 = extractvalue { ptr, i64 } %20, 1
  call void @sere_write(ptr %21, i64 %22)
  call void @sere_write_nl()
  %23 = call i32 @sere_has_error()
  %24 = icmp ne i32 %23, 0
  br i1 %24, label %err10, label %err.ok11

union.str.def:                                    ; preds = %err.ok8
  store { ptr, i64 } { ptr @10, i64 10 }, ptr %union.str, align 8
  br label %union.str.end

union.str.case:                                   ; preds = %err.ok8
  store { ptr, i64 } { ptr @9, i64 4 }, ptr %union.str, align 8
  br label %union.str.end

union.str.case9:                                  ; preds = %err.ok8
  store { ptr, i64 } zeroinitializer, ptr %union.str, align 8
  br label %union.str.end

err10:                                            ; preds = %union.str.end
  ret i32 0

err.ok11:                                         ; preds = %union.str.end
  %25 = call { ptr, i64 } @Result_str_unwrap(ptr %greet2)
  %26 = call i32 @sere_has_error()
  %27 = icmp ne i32 %26, 0
  br i1 %27, label %err12, label %err.ok13

err12:                                            ; preds = %err.ok11
  ret i32 0

err.ok13:                                         ; preds = %err.ok11
  %28 = call i32 @sere_has_error()
  %29 = icmp ne i32 %28, 0
  br i1 %29, label %err14, label %err.ok15

err14:                                            ; preds = %err.ok13
  ret i32 0

err.ok15:                                         ; preds = %err.ok13
  ret i32 0
}

define { ptr, i64 } @Result_str_unwrap(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load { i32, ptr }, ptr %self.slot, align 8
  %1 = extractvalue { i32, ptr } %0, 0
  %2 = icmp eq i32 %1, 0
  br i1 %2, label %match.arm, label %match.next

match.end:                                        ; preds = %match.next2, %err.ok8
  ret { ptr, i64 } zeroinitializer

match.arm:                                        ; preds = %entry
  %3 = extractvalue { i32, ptr } %0, 1
  %value = alloca { ptr, i64 }, align 8
  %4 = getelementptr inbounds nuw { { ptr, i64 } }, ptr %3, i32 0, i32 0
  %5 = load { ptr, i64 }, ptr %4, align 8
  store { ptr, i64 } %5, ptr %value, align 8
  %6 = load { ptr, i64 }, ptr %value, align 8
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

match.next:                                       ; preds = %entry
  %9 = icmp eq i32 %1, 1
  br i1 %9, label %match.arm1, label %match.next2

err:                                              ; preds = %match.arm
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %match.arm
  ret { ptr, i64 } %6

match.arm1:                                       ; preds = %match.next
  %10 = extractvalue { i32, ptr } %0, 1
  %message = alloca { ptr, i64 }, align 8
  %11 = getelementptr inbounds nuw { { ptr, i64 } }, ptr %10, i32 0, i32 0
  %12 = load { ptr, i64 }, ptr %11, align 8
  store { ptr, i64 } %12, ptr %message, align 8
  %init.tmp = alloca %Exception, align 8
  store %Exception zeroinitializer, ptr %init.tmp, align 8
  %13 = getelementptr inbounds nuw %Exception, ptr %init.tmp, i32 0, i32 0
  store i32 -1845790122, ptr %13, align 4
  %14 = load { ptr, i64 }, ptr %message, align 8
  call void @prelude_Exception___init__(ptr %init.tmp, { ptr, i64 } %14)
  %15 = load %Exception, ptr %init.tmp, align 8
  %16 = call i32 @sere_has_error()
  %17 = icmp ne i32 %16, 0
  br i1 %17, label %err3, label %err.ok4

match.next2:                                      ; preds = %match.next
  br label %match.end

err3:                                             ; preds = %match.arm1
  ret { ptr, i64 } zeroinitializer

err.ok4:                                          ; preds = %match.arm1
  %18 = extractvalue %Exception %15, 1
  %19 = extractvalue { ptr, i64 } %18, 0
  %20 = extractvalue { ptr, i64 } %18, 1
  call void @sere_raise(ptr @11, ptr %19, i64 %20)
  %21 = alloca %Exception, align 8
  store %Exception %15, ptr %21, align 8
  call void @sere_error_set_object(ptr %21, i64 24)
  %22 = call i32 @sere_has_error()
  %23 = icmp ne i32 %22, 0
  br i1 %23, label %err5, label %err.ok6

err5:                                             ; preds = %err.ok4
  ret { ptr, i64 } zeroinitializer

err.ok6:                                          ; preds = %err.ok4
  %24 = call i32 @sere_has_error()
  %25 = icmp ne i32 %24, 0
  br i1 %25, label %err7, label %err.ok8

err7:                                             ; preds = %err.ok6
  ret { ptr, i64 } zeroinitializer

err.ok8:                                          ; preds = %err.ok6
  br label %match.end
}

define { i32, i64 } @Result_str_try_unwrap(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load { i32, ptr }, ptr %self.slot, align 8
  %1 = extractvalue { i32, ptr } %0, 0
  %2 = icmp eq i32 %1, 0
  br i1 %2, label %match.arm, label %match.next

match.end:                                        ; preds = %match.next2
  ret { i32, i64 } zeroinitializer

match.arm:                                        ; preds = %entry
  %3 = extractvalue { i32, ptr } %0, 1
  %value = alloca { ptr, i64 }, align 8
  %4 = getelementptr inbounds nuw { { ptr, i64 } }, ptr %3, i32 0, i32 0
  %5 = load { ptr, i64 }, ptr %4, align 8
  store { ptr, i64 } %5, ptr %value, align 8
  %6 = load { ptr, i64 }, ptr %value, align 8
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

match.next:                                       ; preds = %entry
  %9 = icmp eq i32 %1, 1
  br i1 %9, label %match.arm1, label %match.next2

err:                                              ; preds = %match.arm
  ret { i32, i64 } zeroinitializer

err.ok:                                           ; preds = %match.arm
  ret { i32, i64 } { i32 1, i64 0 }

match.arm1:                                       ; preds = %match.next
  %10 = extractvalue { i32, ptr } %0, 1
  %11 = call i32 @sere_has_error()
  %12 = icmp ne i32 %11, 0
  br i1 %12, label %err3, label %err.ok4

match.next2:                                      ; preds = %match.next
  br label %match.end

err3:                                             ; preds = %match.arm1
  ret { i32, i64 } zeroinitializer

err.ok4:                                          ; preds = %match.arm1
  ret { i32, i64 } zeroinitializer
}

define i1 @Result_str_is_ok(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load { i32, ptr }, ptr %self.slot, align 8
  %1 = extractvalue { i32, ptr } %0, 0
  %2 = icmp eq i32 %1, 0
  br i1 %2, label %match.arm, label %match.next

match.end:                                        ; preds = %match.next2
  ret i1 false

match.arm:                                        ; preds = %entry
  %3 = extractvalue { i32, ptr } %0, 1
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

match.next:                                       ; preds = %entry
  %6 = icmp eq i32 %1, 1
  br i1 %6, label %match.arm1, label %match.next2

err:                                              ; preds = %match.arm
  ret i1 false

err.ok:                                           ; preds = %match.arm
  ret i1 true

match.arm1:                                       ; preds = %match.next
  %7 = extractvalue { i32, ptr } %0, 1
  %8 = call i32 @sere_has_error()
  %9 = icmp ne i32 %8, 0
  br i1 %9, label %err3, label %err.ok4

match.next2:                                      ; preds = %match.next
  br label %match.end

err3:                                             ; preds = %match.arm1
  ret i1 false

err.ok4:                                          ; preds = %match.arm1
  ret i1 false
}

define internal void @sere.module.init() {
entry:
  %0 = load i8, ptr @sere.module.init.done, align 1
  %1 = icmp eq i8 %0, 0
  br i1 %1, label %init, label %end

init:                                             ; preds = %entry
  store i8 1, ptr @sere.module.init.done, align 1
  br label %end

end:                                              ; preds = %init, %entry
  ret void
}

declare i32 @sere_has_error()

declare void @sere_free(ptr)

declare void @sere_shared_release(ptr)

declare void @sere_raise(ptr, ptr, i64)

declare void @sere_error_set_object(ptr, i64)

declare i32 @sere_str_cmp(ptr, i64, ptr, i64)

declare ptr @sere_alloc(i64)

declare ptr @sere_str_concat_data(ptr, i64, ptr, i64, ptr)

declare ptr @sere_shared_new(i64)

declare i64 @sere_list_len(ptr)

declare void @sere_write_nl()

declare void @sere_write(ptr, i64)

define i32 @main(i32 %0, ptr %1) {
entry:
  call void @sere_mod_init()
  call void @sere.module.init()
  %2 = call i32 @sere_main()
  call void @sere_error_unhandled()
  ret i32 %2
}

define weak void @sere_mod_init() {
entry:
  ret void
}

declare void @sere_error_unhandled()
