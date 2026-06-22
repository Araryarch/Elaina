namespace ElainaUI.Models
{
    public class ExecutionResult
    {
        public bool Success { get; set; }
        public string Message { get; set; } = string.Empty;
        public string Output { get; set; } = string.Empty;
        public double ExecutionTimeMs { get; set; }
    }
}
