; ModuleID = '_scratch\fold_repro.sere'
source_filename = "_scratch\\fold_repro.sere"
target triple = "x86_64-pc-windows-msvc"

@sere.module.init.done = internal global i8 0
@0 = private unnamed_addr constant [2 x i8] c" \00", align 1
@1 = private unnamed_addr constant [2 x i8] c" \00", align 1
@2 = private unnamed_addr constant [2 x i8] c" \00", align 1
@3 = private unnamed_addr constant [2 x i8] c" \00", align 1
@4 = private unnamed_addr constant [2 x i8] c" \00", align 1
@5 = private unnamed_addr constant [4 x i8] c"yes\00", align 1
@6 = private unnamed_addr constant [2 x i8] c" \00", align 1
@7 = private unnamed_addr constant [3 x i8] c"no\00", align 1

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
  br label %if.then

if.end:                                           ; preds = %err.ok10
  ret void

if.then:                                          ; preds = %err.ok6
  call void @sere_write(ptr @5, i64 3)
  call void @sere_write_nl()
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err7, label %err.ok8

err7:                                             ; preds = %if.then
  ret void

err.ok8:                                          ; preds = %if.then
  %12 = call i32 @sere_has_error()
  %13 = icmp ne i32 %12, 0
  br i1 %13, label %err9, label %err.ok10

err9:                                             ; preds = %err.ok8
  ret void

err.ok10:                                         ; preds = %err.ok8
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
