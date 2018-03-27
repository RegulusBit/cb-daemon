#include "logger.h"




src::logger_mt& init()
{
    logging::add_file_log
    (
        keywords::file_name = "Logs/logfile_%N.log",
        keywords::rotation_size = 10 * 1024 * 1024,                                   
        keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
        keywords::format = "[%TimeStamp%]: %Message%"
    );

    logging::add_common_attributes();
    src::logger_mt& lg = my_logger::get();
    return lg;
}