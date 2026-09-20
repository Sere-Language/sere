; ModuleID = '_scratch\fold_repro.sere'
source_filename = "_scratch\\fold_repro.sere"
target triple = "x86_64-pc-windows-msvc"

%Exception = type { i32, { ptr, i64 } }

@sere.module.init.done = internal global i8 0
@0 = private unnamed_addr constant [2 x i8] c" \00", align 1
@1 = private unnamed_addr constant [2 x i8] c" \00", align 1
@2 = private unnamed_addr constant [2 x i8] c" \00", align 1
@3 = private unnamed_addr constant [2 x i8] c" \00", align 1
@4 = private unnamed_addr constant [2 x i8] c" \00", align 1
@5 = private unnamed_addr constant [4 x i8] c"yes\00", align 1
@6 = private unnamed_addr constant [2 x i8] c" \00", align 1
@7 = private unnamed_addr constant [3 x i8] c"no\00", align 1

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

define void @sere_main() {
entry:
  %x = alloca i32, align 4
  call void @sere.module.init()
  call void @sere_write_i32(i32 2)
  call void @sere_write_nl()
  %0 = call i32 @sere_has_error()
  %1 = icmp ne i32 %0, 0
  br i1 %1, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  call void @sere_write_i32(i32 10)
  call void @sere_write_nl()
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err1, label %err.ok2

err1:                                             ; preds = %err.ok
  ret void

err.ok2:                                          ; preds = %err.ok
  call void @sere_write_i32(i32 7)
  call void @sere_write_nl()
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok2
  ret void

err.ok4:                                          ; preds = %err.ok2
  store i32 7, ptr %x, align 4
  %6 = load i32, ptr %x, align 4
  %7 = add i32 %6, 1
  call void @sere_write_i32(i32 %7)
  call void @sere_write_nl()
  %8 = call i32 @sere_has_error()
  %9 = icmp ne i32 %8, 0
  br i1 %9, label %err5, label %err.ok6

err5:                                             ; preds = %err.ok4
  ret void

err.ok6:                                          ; preds = %err.ok4
  br i1 true, label %if.then, label %if.next

if.end:                                           ; preds = %err.ok14, %err.ok10
  ret void

if.then:                                          ; preds = %err.ok6
  call void @sere_write(ptr @5, i64 3)
  call void @sere_write_nl()
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err7, label %err.ok8

if.next:                                          ; preds = %err.ok6
  call void @sere_write(ptr @7, i64 2)
  call void @sere_write_nl()
  %12 = call i32 @sere_has_error()
  %13 = icmp ne i32 %12, 0
  br i1 %13, label %err11, label %err.ok12

err7:                                             ; preds = %if.then
  ret void

err.ok8:                                          ; preds = %if.then
  %14 = call i32 @sere_has_error()
  %15 = icmp ne i32 %14, 0
  br i1 %15, label %err9, label %err.ok10

err9:                                             ; preds = %err.ok8
  ret void

err.ok10:                                         ; preds = %err.ok8
  br label %if.end

err11:                                            ; preds = %if.next
  ret void

err.ok12:                                         ; preds = %if.next
  %16 = call i32 @sere_has_error()
  %17 = icmp ne i32 %16, 0
  br i1 %17, label %err13, label %err.ok14

err13:                                            ; preds = %err.ok12
  ret void

err.ok14:                                         ; preds = %err.ok12
  br label %if.end
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

declare ptr @sere_alloc(i64)

declare ptr @sere_shared_new(i64)

declare i64 @sere_list_len(ptr)

declare void @sere_write_nl()

declare void @sere_write_i32(i32)

declare void @sere_write(ptr, i64)

define i32 @main(i32 %0, ptr %1) {
entry:
  call void @sere_mod_init()
  call void @sere.module.init()
  call void @sere_main()
  call void @sere_error_unhandled()
  ret i32 0
}

define weak void @sere_mod_init() {
entry:
  ret void
}

declare void @sere_error_unhandled()
