// Ghidra headless script: decompile every function of the program into one text file (argument: the output path,
// required).
// Used for the reverse-engineering of Virus Control; the output stays in work/ (never committed).
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import java.io.PrintWriter;

public class DecompileAll extends GhidraScript {
    @Override
    public void run() throws Exception {
        // no default: a decomp.c in the current folder could end up in the repository (rule 1: no Access code)
        if (getScriptArgs().length == 0 || getScriptArgs()[0].isEmpty()) {
            printerr("DecompileAll: give the output path (under work/), e.g. -postScript DecompileAll.java work/decomp/x.c");
            throw new IllegalArgumentException("DecompileAll: output path missing");
        }
        String out = getScriptArgs()[0];
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        try (PrintWriter w = new PrintWriter(out, "UTF-8")) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (monitor.isCancelled())
                    break;
                DecompileResults r = d.decompileFunction(f, 60, monitor);
                w.println("// ==== " + f.getEntryPoint() + " " + f.getName(true));
                w.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// decompile failed: " + r.getErrorMessage());
            }
        }
    }
}
